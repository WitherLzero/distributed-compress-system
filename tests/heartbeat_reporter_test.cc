// heartbeat_reporter_test —— CP5：server 心跳客户端。
// 用真 MasterRuntime 起服务端（心跳周期 1s），reporter 是被测件；
// 断言三件事：首跳即注册、按响应周期持续刷新、Stop 及时打断等待。
#include <atomic>
#include <chrono>
#include <functional>

#include <brpc/channel.h>
#include <gtest/gtest.h>

#include "compress/options.h"
#include "heartbeat_reporter.h"
#include "master_runtime.h"

namespace {

using namespace std::chrono;
using namespace compress;

class HeartbeatReporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Options opts;
    opts.heartbeat_interval_sec = 1;   // beat_interval_ms 合同 = 1000
    opts.death_ttl_sec = 4;
    opts.death_scan_interval_sec = 1;
    runtime_ = std::make_unique<MasterRuntime>(opts);
    ASSERT_GT(runtime_->Start(0), 0);

    brpc::ChannelOptions chan_opts;
    chan_opts.timeout_ms = 2000;
    const std::string addr = "127.0.0.1:" + std::to_string(runtime_->listen_port());
    ASSERT_EQ(channel_.Init(addr.c_str(), &chan_opts), 0);
  }
  void TearDown() override {
    if (reporter_) reporter_->Stop();
    runtime_->Stop();
  }

  // 轮询直到 pred 成立或超时（心跳是异步线程推的，测试只能观察收敛）。
  bool PollUntil(const std::function<bool()>& pred, milliseconds budget) {
    const auto deadline = steady_clock::now() + budget;
    while (steady_clock::now() < deadline) {
      if (pred()) return true;
      std::this_thread::sleep_for(milliseconds(20));
    }
    return pred();
  }

  int WorkersCount() {
    system::MasterService_Stub stub(&channel_);
    brpc::Controller cntl;
    system::WorkersInfoRequest req;
    system::WorkersInfoResponse resp;
    stub.GetWorkersInfo(&cntl, &req, &resp, nullptr);
    if (cntl.Failed()) return -1;
    return resp.workers_size();
  }

  std::unique_ptr<MasterRuntime> runtime_;
  brpc::Channel channel_;
  std::unique_ptr<HeartbeatReporter> reporter_;
  std::atomic<std::uint32_t> queue_len_{0};
};

// 1) 启动即发首跳：不等一个周期，500ms 内 master 列表可见本实例
TEST_F(HeartbeatReporterTest, FirstBeatRegistersImmediately) {
  reporter_ = std::make_unique<HeartbeatReporter>(
      "127.0.0.1:" + std::to_string(runtime_->listen_port()), "srv-a", 8002,
      [this] { return LoadSample{10, 20, queue_len_.load()}; });
  reporter_->Start();

  EXPECT_TRUE(PollUntil([this] { return WorkersCount() == 1; }, milliseconds(500)))
      << "first beat did not register within 500ms, count=" << WorkersCount();
}

// 2) 持续刷新：改负载值后，列表里该实例的字段随下一次心跳更新（>=2 次心跳）
TEST_F(HeartbeatReporterTest, HeartbeatsRefreshContinuously) {
  reporter_ = std::make_unique<HeartbeatReporter>(
      "127.0.0.1:" + std::to_string(runtime_->listen_port()), "srv-a", 8002,
      [this] { return LoadSample{10, 20, queue_len_.load()}; });
  reporter_->Start();
  ASSERT_TRUE(PollUntil([this] { return WorkersCount() == 1; }, seconds(3)));

  queue_len_.store(9);
  EXPECT_TRUE(PollUntil([this] {
    system::MasterService_Stub stub(&channel_);
    brpc::Controller cntl;
    system::WorkersInfoRequest req;
    system::WorkersInfoResponse resp;
    stub.GetWorkersInfo(&cntl, &req, &resp, nullptr);
    return !cntl.Failed() && resp.workers_size() == 1 &&
           resp.workers(0).queue_len() == 9;
  }, seconds(3))) << "load update never propagated; still count/ql: "
                  << WorkersCount();
}

// 3) Stop 及时：不等到下个周期结束（< 900ms，周期是 1000ms）
TEST_F(HeartbeatReporterTest, StopInterruptsWaitPromptly) {
  reporter_ = std::make_unique<HeartbeatReporter>(
      "127.0.0.1:" + std::to_string(runtime_->listen_port()), "srv-a", 8002,
      [this] { return LoadSample{10, 20, queue_len_.load()}; });
  reporter_->Start();
  ASSERT_TRUE(PollUntil([this] { return WorkersCount() == 1; }, seconds(3)));

  const auto t0 = steady_clock::now();
  reporter_->Stop();
  const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - t0);
  EXPECT_LT(elapsed, milliseconds(900))
      << "Stop took " << elapsed.count() << "ms; wait not interrupted";
}

}  // namespace
