// master_smoke_test —— CP3：BRPC 服务端填肉后的端到端列表流转。
// 装配线与 main.cpp 同构（同一 MasterRuntime），假时钟注入：时间即测试输入。
// server 模块尚不存在（W5）——测试的 Channel 代码就是未来 server 注册/心跳路径的同构体。
#include <atomic>
#include <brpc/channel.h>
#include <brpc/server.h>
#include <gtest/gtest.h>

#include "compress/options.h"
#include "master_runtime.h"

namespace {

using namespace std::chrono;
using namespace compress;

class MasterFlowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Options opts;  // 默认：ttl=20s，扫描 2s
    runtime_ = std::make_unique<MasterRuntime>(opts, [this] { return now_.load(); });
    ASSERT_GT(runtime_->Start(0), 0);  // 返回实际监听端口
    port_ = runtime_->listen_port();

    brpc::ChannelOptions chan_opts;
    chan_opts.timeout_ms = 2000;
    const std::string addr = "127.0.0.1:" + std::to_string(port_);
    ASSERT_EQ(channel_.Init(addr.c_str(), &chan_opts), 0);
  }
  void TearDown() override { runtime_->Stop(); }

  std::unique_ptr<MasterRuntime> runtime_;
  std::atomic<int64_t> now_{1000};  // 假时钟：扫描线程读 / 主线程写
  int port_ = 0;
  brpc::Channel channel_;
};

// 1) 注册即出现：首报心跳 = 注册，查询返回该实例且字段随心跳上报
TEST_F(MasterFlowTest, RegisterThenAppearsInWorkersInfo) {
  system::RegistryService_Stub reg_stub(&channel_);
  brpc::Controller c1;
  system::HeartbeatRequest hb;
  hb.set_server_id("srv-1");
  hb.set_port(8002);
  hb.set_queue_len(3);
  hb.set_cpu_percent(42);
  hb.set_mem_percent(55);
  system::HeartbeatResponse hb_resp;
  reg_stub.Heartbeat(&c1, &hb, &hb_resp, nullptr);
  ASSERT_FALSE(c1.Failed()) << c1.ErrorText();
  EXPECT_TRUE(hb_resp.accepted());
  EXPECT_EQ(hb_resp.beat_interval_ms(), 5000);  // Options::heartbeat_interval_sec=5

  system::MasterService_Stub master_stub(&channel_);
  brpc::Controller c2;
  system::WorkersInfoRequest q;
  system::WorkersInfoResponse resp;
  master_stub.GetWorkersInfo(&c2, &q, &resp, nullptr);
  ASSERT_FALSE(c2.Failed()) << c2.ErrorText();
  ASSERT_EQ(resp.workers_size(), 1);
  const auto& w = resp.workers(0);
  EXPECT_EQ(w.server_id(), "srv-1");
  EXPECT_EQ(w.port(), 8002);
  EXPECT_EQ(w.queue_len(), 3);
  EXPECT_EQ(w.last_hb_ms(), 1000);  // 假时钟当前值
}

// 2) 同址心跳刷新不重复：条目数不变，负载字段取最新上报
TEST_F(MasterFlowTest, HeartbeatRefreshesNotDuplicates) {
  system::RegistryService_Stub reg_stub(&channel_);
  system::MasterService_Stub master_stub(&channel_);
  for (int ql : {3, 7}) {
    brpc::Controller c;
    system::HeartbeatRequest hb;
    hb.set_server_id("srv-1");
    hb.set_port(8002);
    hb.set_queue_len(ql);
    system::HeartbeatResponse resp;
    reg_stub.Heartbeat(&c, &hb, &resp, nullptr);
    ASSERT_FALSE(c.Failed()) << c.ErrorText();
    EXPECT_TRUE(resp.accepted());
  }
  brpc::Controller cq;
  system::WorkersInfoRequest q;
  system::WorkersInfoResponse resp;
  master_stub.GetWorkersInfo(&cq, &q, &resp, nullptr);
  ASSERT_FALSE(cq.Failed()) << cq.ErrorText();
  ASSERT_EQ(resp.workers_size(), 1);
  EXPECT_EQ(resp.workers(0).queue_len(), 7);
}

// 3) TTL 内消失：时间快进超过判死阈值，该实例从查询结果消失（即使扫描线程未摘除）
TEST_F(MasterFlowTest, WorkerVanishesAfterTtl) {
  system::RegistryService_Stub reg_stub(&channel_);
  brpc::Controller c1;
  system::HeartbeatRequest hb;
  hb.set_server_id("srv-1");
  hb.set_port(8002);
  system::HeartbeatResponse r1;
  reg_stub.Heartbeat(&c1, &hb, &r1, nullptr);
  ASSERT_FALSE(c1.Failed()) << c1.ErrorText();

  now_.store(1000 + 20000 + 1);  // ttl=20s 刚好越线（严格：hb > floor 才活）

  // 扫描线程摘除 + 查询惰性过滤双保险：轮询至列表空（deadline 2s）
  system::MasterService_Stub master_stub(&channel_);
  bool gone = false;
  for (int i = 0; i < 100 && !gone; ++i) {
    brpc::Controller cq;
    system::WorkersInfoRequest q;
    system::WorkersInfoResponse resp;
    master_stub.GetWorkersInfo(&cq, &q, &resp, nullptr);
    ASSERT_FALSE(cq.Failed()) << cq.ErrorText();
    gone = resp.workers_size() == 0;
    if (!gone) std::this_thread::sleep_for(milliseconds(20));
  }
  EXPECT_TRUE(gone) << "expired worker still listed after ttl deadline";
}

// 4) 空注册表安全：无注册实例时返回空表
TEST_F(MasterFlowTest, EmptyRegistryReturnsEmptyList) {
  system::MasterService_Stub master_stub(&channel_);
  brpc::Controller c;
  system::WorkersInfoRequest q;
  system::WorkersInfoResponse resp;
  master_stub.GetWorkersInfo(&c, &q, &resp, nullptr);
  ASSERT_FALSE(c.Failed()) << c.ErrorText();
  EXPECT_EQ(resp.workers_size(), 0);
}

}  // namespace
