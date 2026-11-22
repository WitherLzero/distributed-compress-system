// death_scanner_test —— 判死扫描线程：时机策略层，事实谓词在 ServerRegistry（CP1）。
// 时间源注入：Clock 假时钟推进，测试零真实 sleep 依赖（扫描周期除外，见第二用例）。
// scanner 一律堆上构造（unique_ptr）：其 mutex 落堆地址，避免 TSAN 栈地址复用误报。
#include "death_scanner.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <gtest/gtest.h>
#include <thread>

#include "server_registry.h"

namespace {

using namespace std::chrono;
using namespace compress;

WorkerInfo MakeWorker(const std::string& ep, int64_t hb) {
  WorkerInfo w;
  w.endpoint = ep;
  w.last_heartbeat_ms = hb;
  return w;
}

// 1) 只摘过期，保留存活：Start 后首扫即验证（RunOnce 收 private，改由线程驱动）
TEST(DeathScannerTest, SweepRemovesOnlyExpired) {
  ServerRegistry reg;
  // fake_now=12000, ttl=5000 → floor=7000：hb=11000 存活，hb=1000 过期
  reg.Upsert(MakeWorker("10.0.0.1:8000", /*hb=*/11000));  // 存活
  reg.Upsert(MakeWorker("10.0.0.2:8000", /*hb=*/1000));   // 过期

  std::atomic<int64_t> fake_now{12000};
  auto scanner = std::make_unique<DeathScanner>(
      reg, [&] { return fake_now.load(); },
      /*ttl=*/milliseconds(5000), /*interval=*/milliseconds(20));
  scanner->Start();
  // 轮询等首扫摘除过期条目（deadline 2s，与时序解耦）
  bool swept = false;
  for (int i = 0; i < 100 && !swept; ++i) {
    swept = reg.HealthyWorkers(fake_now.load(), 5000).size() == 1;
    if (!swept) std::this_thread::sleep_for(milliseconds(20));
  }
  EXPECT_TRUE(swept) << "expired entry not swept";
  auto alive = reg.HealthyWorkers(fake_now.load(), /*ttl=*/5000);
  ASSERT_EQ(alive.size(), 1u);
  EXPECT_EQ(alive[0].endpoint, "10.0.0.1:8000");
  scanner->Stop();
}

// 2) Start 后周期自动清扫：假时钟推进，过期条目在一个扫描周期内消失
TEST(DeathScannerTest, StartPeriodicallySweeps) {
  ServerRegistry reg;
  std::atomic<int64_t> fake_now{1000};  // 扫描线程读 / 主线程写：原子防撕裂
  reg.Upsert(MakeWorker("10.0.0.1:8000", /*hb=*/1000));

  auto scanner = std::make_unique<DeathScanner>(
      reg, [&] { return fake_now.load(); },
      /*ttl=*/milliseconds(5000), /*interval=*/milliseconds(20));
  scanner->Start();
  fake_now.store(12000);  // 时间跳跃：hb=1000 已过期 (1000 <= 12000-5000)

  // 轮询等扫描线程摘除（deadline 2s，避免与时序耦合的硬 sleep）
  bool removed = false;
  for (int i = 0; i < 100 && !removed; ++i) {
    removed = reg.HealthyWorkers(fake_now.load(), 5000).empty();
    if (!removed) std::this_thread::sleep_for(milliseconds(20));
  }
  EXPECT_TRUE(removed) << "expired entry not swept after deadline";
  scanner->Stop();
}

// 3) Stop 及时返回且不再扫描：线程停在长 interval 等待中，notify 后立即醒
TEST(DeathScannerTest, StopIsPromptAndQuiesces) {
  ServerRegistry reg;
  std::atomic<int> clock_calls{0};
  auto scanner = std::make_unique<DeathScanner>(
      reg, [&] { clock_calls.fetch_add(1); return int64_t{1000}; },
      /*ttl=*/milliseconds(5000), /*interval=*/milliseconds(3600000));
  scanner->Start();
  // 等首轮扫描发生（clock 至少被调一次）
  for (int i = 0; i < 100 && clock_calls.load() == 0; ++i)
    std::this_thread::sleep_for(milliseconds(10));
  EXPECT_GE(clock_calls.load(), 1);

  std::this_thread::sleep_for(milliseconds(50));  // 确保 T 已入睡（探针）
  auto t0 = steady_clock::now();
  scanner->Stop();
  auto stop_ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
  EXPECT_LT(stop_ms, 1000) << "Stop should wake the wait immediately";

  int after = clock_calls.load();
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(clock_calls.load(), after) << "no scans after Stop";
}

}  // namespace
