// server_runtime_test —— CP6：ServerRuntime 组合根端到端。
// 真 MasterRuntime + 真 ServerRuntime 同进程：注册 / 压缩 / 停机三条路径。
#include <atomic>
#include <chrono>
#include <functional>

#include <brpc/channel.h>
#include <gtest/gtest.h>

#include "compress/options.h"
#include "master_runtime.h"
#include "server_runtime.h"

namespace {

using namespace std::chrono;
using namespace compress;

class ServerRuntimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Options mopts;
    mopts.heartbeat_interval_sec = 1;
    mopts.death_ttl_sec = 4;
    mopts.death_scan_interval_sec = 1;
    master_ = std::make_unique<MasterRuntime>(mopts);
    ASSERT_GT(master_->Start(0), 0);
    master_addr_ = "127.0.0.1:" + std::to_string(master_->listen_port());

    brpc::ChannelOptions chan_opts;
    chan_opts.timeout_ms = 2000;
    ASSERT_EQ(master_chan_.Init(master_addr_.c_str(), &chan_opts), 0);
  }
  void TearDown() override {
    if (server_) server_->Stop();
    master_->Stop();
  }

  bool PollUntil(const std::function<bool()>& pred, milliseconds budget) {
    const auto deadline = steady_clock::now() + budget;
    while (steady_clock::now() < deadline) {
      if (pred()) return true;
      std::this_thread::sleep_for(milliseconds(20));
    }
    return pred();
  }

  int WorkersCount() {
    system::MasterService_Stub stub(&master_chan_);
    brpc::Controller cntl;
    system::WorkersInfoRequest req;
    system::WorkersInfoResponse resp;
    stub.GetWorkersInfo(&cntl, &req, &resp, nullptr);
    if (cntl.Failed()) return -1;
    return resp.workers_size();
  }

  // 直连 server 的 Compress：验证装配后的压缩端点是活的
  bool CompressRoundTrip(int server_port) {
    brpc::Channel ch;
    brpc::ChannelOptions opts;
    opts.timeout_ms = 5000;
    if (ch.Init(("127.0.0.1:" + std::to_string(server_port)).c_str(), &opts) != 0) {
      return false;
    }
    system::CompressService_Stub stub(&ch);
    std::string payload(1 << 20, 'x');  // 1MB 周期字符，可压
    brpc::Controller cntl;
    system::CompressRequest req;
    req.set_level(system::SHALLOW);
    cntl.request_attachment().append(payload);
    system::CompressResponse resp;
    stub.Compress(&cntl, &req, &resp, nullptr);
    if (cntl.Failed() || resp.result() != system::OK) return false;
    if (resp.compressed_len() == 0 ||
        static_cast<size_t>(resp.compressed_len()) != cntl.response_attachment().size()) {
      return false;
    }
    return true;  // round-trip 细节由 server_compress_test 兜底，这里验"链路通 + 字段一致"
  }

  std::unique_ptr<MasterRuntime> master_;
  std::unique_ptr<ServerRuntime> server_;
  std::string master_addr_;
  brpc::Channel master_chan_;
};

// 1) 端到端：server 启动 → 首跳注册进真 master → 压缩端点对外可用
TEST_F(ServerRuntimeTest, RegistersWithRealMasterAndServesCompress) {
  Options sopts;
  sopts.master_addr = master_addr_;
  server_ = std::make_unique<ServerRuntime>(sopts);
  ASSERT_GT(server_->Start(0), 0);

  EXPECT_TRUE(PollUntil([this] { return WorkersCount() == 1; }, seconds(3)))
      << "server did not register with master, count=" << WorkersCount();
  EXPECT_TRUE(CompressRoundTrip(server_->listen_port()));
}

// 2) worker_count=0 = 按硬件并发自动（Options 惯例）：装配不崩、压缩链路可用
TEST_F(ServerRuntimeTest, AutoWorkerCountFallsBackToHardware) {
  Options sopts;
  sopts.master_addr = master_addr_;
  sopts.server_worker_count = 0;  // 0 = auto
  server_ = std::make_unique<ServerRuntime>(sopts);
  ASSERT_GT(server_->Start(0), 0);
  EXPECT_TRUE(CompressRoundTrip(server_->listen_port()))
      << "auto worker count must yield at least one worker";
}

// 3) Stop 幂等不挂死：连停两次，第二次是 no-op
TEST_F(ServerRuntimeTest, StopIsIdempotentAndDoesNotHang) {
  Options sopts;
  sopts.master_addr = master_addr_;
  server_ = std::make_unique<ServerRuntime>(sopts);
  ASSERT_GT(server_->Start(0), 0);

  server_->Stop();
  const auto t0 = steady_clock::now();
  server_->Stop();  // 第二次必须是 no-op
  const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - t0);
  EXPECT_LT(elapsed, milliseconds(500)) << "second Stop took " << elapsed.count() << "ms";
}

}  // namespace
