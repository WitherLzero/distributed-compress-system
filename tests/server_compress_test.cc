// server_compress_test —— CP4：Compress 端点接线（延迟 done 模式）。
// 同进程回环：brpc::Server + Channel，真 RPC 走 attachment；WorkerPool 队列由用例
// 自由配形（worker 数 0/1/2 × 容量 1/64），覆盖四契约：
//   1) round-trip（浅/深）2) 队满 brpc::ELIMIT 3) 停机排干不挂死 4) 句柄生命周期合规
#include <gtest/gtest.h>
#include <brpc/channel.h>
#include <brpc/server.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "compress/codec.h"
#include "compress_service_impl.h"
#include "worker_pool.h"

namespace {

using namespace compress;
using system::CompressRequest;
using system::CompressResponse;

// incompressible=false：周期字母（可压，LZ4 变小）；
// incompressible=true：固定种子伪随机（LZMA 慢，用于占住 worker 的确定性时序）。
std::string MakePayload(std::size_t bytes, bool incompressible) {
  std::string s(bytes, '\0');
  if (incompressible) {
    std::mt19937 rng(42);
    for (auto& ch : s) ch = static_cast<char>(rng() & 0xff);
  } else {
    for (std::size_t i = 0; i < bytes; ++i) s[i] = static_cast<char>('a' + i % 26);
  }
  return s;
}

class ServerCompressTest : public ::testing::Test {
 protected:
  // worker 数与队列容量由用例决定，SetUp 不配服务，按需启动
  void StartService(std::size_t worker_count, std::size_t queue_capacity) {
    pool_ = std::make_unique<WorkerPool>(worker_count, queue_capacity);
    service_ = std::make_unique<CompressServiceImpl>(*pool_);
    ASSERT_EQ(server_.AddService(service_.get(), brpc::SERVER_DOESNT_OWN_SERVICE), 0);
    ASSERT_EQ(server_.Start(0, nullptr), 0);
    const int port = server_.listen_address().port;

    brpc::ChannelOptions chan_opts;
    chan_opts.timeout_ms = 15000;  // 深压 4MB 随机数据可达秒级
    const std::string addr = "127.0.0.1:" + std::to_string(port);
    ASSERT_EQ(channel_.Init(addr.c_str(), &chan_opts), 0);
    stub_ = std::make_unique<system::CompressService_Stub>(&channel_);
  }

  void TearDown() override {
    if (pool_) pool_->Stop();  // 先排干存量并补 Run，再关 server——顺序是语义
    server_.Stop(0);
    server_.Join();
  }

  // 同步调用（client 轴恒 nullptr），attachment 带入原始数据
  void Call(brpc::Controller* cntl, CompressLevel level, const std::string& payload,
            CompressResponse* resp) {
    CompressRequest req;
    req.set_data_id("blk-test");
    req.set_origin_len(payload.size());
    req.set_level(level == CompressLevel::kDeep ? system::DEEP : system::SHALLOW);
    cntl->request_attachment().append(payload);
    stub_->Compress(cntl, &req, resp, nullptr);
  }

  std::unique_ptr<WorkerPool> pool_;
  std::unique_ptr<CompressServiceImpl> service_;
  brpc::Server server_;
  brpc::Channel channel_;
  std::unique_ptr<system::CompressService_Stub> stub_;
};

// 1) 浅压 round-trip：4MB 可压数据进 → 结果出 → 解压还原逐字节一致
TEST_F(ServerCompressTest, ShallowRoundTripFourMiB) {
  StartService(2, 64);
  const std::string payload = MakePayload(4u << 20, false);

  brpc::Controller c;
  CompressResponse resp;
  Call(&c, CompressLevel::kShallow, payload, &resp);
  ASSERT_FALSE(c.Failed()) << c.ErrorText();
  EXPECT_EQ(resp.result(), system::OK);
  EXPECT_GT(resp.compressed_len(), 0u);
  EXPECT_LT(c.response_attachment().size(), payload.size());  // 可压数据确实变小

  std::string compressed;
  c.response_attachment().copy_to(&compressed);
  EXPECT_EQ(compressed.size(), resp.compressed_len());  // len 与附件实际大小一致

  auto codec = CodecFactory::Create(CompressLevel::kShallow);
  ASSERT_NE(codec, nullptr);
  std::vector<std::uint8_t> out(payload.size());
  std::size_t out_len = 0;
  ASSERT_EQ(codec->Decompress(reinterpret_cast<const std::uint8_t*>(compressed.data()),
                              compressed.size(), out.data(), out.size(), &out_len),
            CompressErr::kOk);
  ASSERT_EQ(out_len, payload.size());
  EXPECT_EQ(std::memcmp(out.data(), payload.data(), out_len), 0);
}

// 2) 深压 round-trip：LZMA 通路同样成立（小样本，只验正确性不卡时长）
TEST_F(ServerCompressTest, DeepRoundTripSmall) {
  StartService(1, 8);
  const std::string payload = MakePayload(256u << 10, true);

  brpc::Controller c;
  CompressResponse resp;
  Call(&c, CompressLevel::kDeep, payload, &resp);
  ASSERT_FALSE(c.Failed()) << c.ErrorText();
  ASSERT_EQ(resp.result(), system::OK);

  std::string compressed;
  c.response_attachment().copy_to(&compressed);
  auto codec = CodecFactory::Create(CompressLevel::kDeep);
  ASSERT_NE(codec, nullptr);
  std::vector<std::uint8_t> out(payload.size());
  std::size_t out_len = 0;
  ASSERT_EQ(codec->Decompress(reinterpret_cast<const std::uint8_t*>(compressed.data()),
                              compressed.size(), out.data(), out.size(), &out_len),
            CompressErr::kOk);
  ASSERT_EQ(out_len, payload.size());
  EXPECT_EQ(std::memcmp(out.data(), payload.data(), out_len), 0);
}

// 3) 队满即拒：1 worker + 1 容量，第三发必须在 RPC 层吃 brpc::ELIMIT（§5.5）
//    R1（4MB 随机数据深压，秒级）占住 worker，R2 占住队列，R3 主线程立即被拒
TEST_F(ServerCompressTest, QueueFullRejectedWithElimit) {
  StartService(1, 1);
  const std::string payload = MakePayload(4u << 20, true);

  brpc::Controller c1;
  CompressResponse r1;
  std::thread t1([&] { Call(&c1, CompressLevel::kDeep, payload, &r1); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));  // R1 抵达并占住 worker

  brpc::Controller c2;
  CompressResponse r2;
  std::thread t2([&] { Call(&c2, CompressLevel::kDeep, payload, &r2); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));  // R2 抵达并入队

  brpc::Controller c3;
  CompressResponse r3;
  Call(&c3, CompressLevel::kShallow, MakePayload(64, false), &r3);  // R3：应立即 brpc::ELIMIT
  EXPECT_TRUE(c3.Failed());
  EXPECT_EQ(c3.ErrorCode(), brpc::ELIMIT);

  t1.join();
  t2.join();
  // 队列里排到的两发都正常完成（不是被误拒）
  EXPECT_FALSE(c1.Failed()) << c1.ErrorText();
  EXPECT_EQ(r1.result(), system::OK);
  EXPECT_FALSE(c2.Failed()) << c2.ErrorText();
  EXPECT_EQ(r2.result(), system::OK);
}

// 4) 停机排干不挂死：0 worker 让任务滞留队列，Stop 后 client 收到 FAILED 而非悬挂
TEST_F(ServerCompressTest, StopFailsQueuedTasksWithoutHanging) {
  StartService(0, 1);
  const std::string payload = MakePayload(64, false);

  brpc::Controller c;
  CompressResponse r;
  std::thread t([&] { Call(&c, CompressLevel::kShallow, payload, &r); });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 任务已入队滞留

  pool_->Stop();  // 排干：滞留任务补 Run（FAILED 响应）
  t.join();

  EXPECT_FALSE(c.Failed()) << c.ErrorText();  // 链路通，业务层失败
  EXPECT_EQ(r.result(), system::FAILED);
}

}  // namespace
