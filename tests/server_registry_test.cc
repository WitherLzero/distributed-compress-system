// server_registry_test.cc —— CP1：注册表纯逻辑四态流转（注册/续期/判死/查询）+ 并发不变量。
// 接口契约：注册表是被动组件（设计文档 §3.2）——判死的"事实"在这里（now+ttl 谓词），
// 判死的"时机"（谁调、多久调）归 CP2 的 DeathScanner。
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <set>
#include <algorithm>

#include "server_registry.h"

namespace {

using compress::ServerRegistry;
using compress::WorkerInfo;

WorkerInfo MakeWorker(const std::string& endpoint, int64_t hb_ms) {
  WorkerInfo w;
  w.endpoint = endpoint;
  w.last_heartbeat_ms = hb_ms;
  return w;
}

TEST(ServerRegistryTest, UpsertNewWorkerAppearsInHealthyList) {
  ServerRegistry reg;
  reg.Upsert(MakeWorker("127.0.0.1:8001", /*hb=*/1000));  // 心跳发生在 t=1000
  // 查询发生在 t=3000，ttl=2500：floor=500，最近心跳 1000 > 500 → 活
  auto workers = reg.HealthyWorkers(/*now=*/3000, /*ttl=*/2500);
  ASSERT_EQ(workers.size(), 1u);
  EXPECT_EQ(workers[0].endpoint, "127.0.0.1:8001");
}

TEST(ServerRegistryTest, UpsertSameEndpointRefreshesNotDuplicates) {
  ServerRegistry reg;
  reg.Upsert(MakeWorker("127.0.0.1:8001", /*hb=*/1000));
  reg.Upsert(MakeWorker("127.0.0.1:8001", /*hb=*/2000));  // 心跳续期
  auto workers = reg.HealthyWorkers(/*now=*/3000, /*ttl=*/2500);  // floor=500 < hb=2000 → 活
  ASSERT_EQ(workers.size(), 1u);
  EXPECT_EQ(workers[0].last_heartbeat_ms, 2000);  // 时间戳被刷新
}

TEST(ServerRegistryTest, ExpiredWorkerIsFilteredOut) {
  ServerRegistry reg;
  reg.Upsert(MakeWorker("127.0.0.1:8001", /*hb=*/1000));
  // now=7000, ttl=5000：最近心跳 1000 < 7000-5000=2000 → 判死
  EXPECT_TRUE(reg.HealthyWorkers(7000, 5000).empty());
  // 边界：now-ttl == hb 恰好判死（存活条件是 hb > floor 严格大于）
  EXPECT_TRUE(reg.HealthyWorkers(6000, 5000).empty());
  // 再老 1ms 的 now：floor=999 < hb=1000 → 存活
  EXPECT_EQ(reg.HealthyWorkers(5999, 5000).size(), 1u);
}

TEST(ServerRegistryTest, RemoveExpiredDropsOnlyStaleEntries) {
  ServerRegistry reg;
  reg.Upsert(MakeWorker("127.0.0.1:8001", /*hb=*/1000));  // 早期心跳，会过期
  reg.Upsert(MakeWorker("127.0.0.1:8002", /*hb=*/9000));  // 近期心跳，仍新鲜
  reg.RemoveExpired(/*now=*/12000, /*ttl=*/5000);  // floor=7000：1000 判死、9000 存活
  auto workers = reg.HealthyWorkers(12000, 5000);
  ASSERT_EQ(workers.size(), 1u);
  EXPECT_EQ(workers[0].endpoint, "127.0.0.1:8002");
}

TEST(ServerRegistryTest, EmptyRegistryIsSafe) {
  ServerRegistry reg;
  EXPECT_TRUE(reg.HealthyWorkers(0, 5000).empty());
  reg.RemoveExpired(0, 5000);  // 不崩即可
  EXPECT_TRUE(reg.HealthyWorkers(0, 5000).empty());
}

// 并发不变量：W 写者 + R 读者并发跑，读者永远看到
// ① endpoint 集合无重复 ② 时间戳 <= 传入 now ③ 不崩（TSAN 下不报竞态）。
TEST(ServerRegistryTest, ConcurrentUpsertAndQueryHoldsInvariants) {
  ServerRegistry reg;
  constexpr int kWriters = 8;
  constexpr int kReaders = 6;
  constexpr int kIters = 500;
  std::atomic<bool> stop{false};
  std::vector<std::thread> threads;

  for (int w = 0; w < kWriters; ++w) {
    threads.emplace_back([&reg, w] {
      for (int i = 0; i < kIters; ++i) {
        // 每个 writer 只写自己的一组 endpoint，其中 1/3 概率重复写（模拟续期）
        const std::string ep =
            "10.0.0." + std::to_string(w) + ":" + std::to_string(i % 170);
        reg.Upsert(MakeWorker(ep, i + 1));
      }
    });
  }
  for (int r = 0; r < kReaders; ++r) {
    threads.emplace_back([&reg, &stop, kWriters, kIters] {
      int64_t now = 100000;
      // 有界轮次 + yield：忙等式无间隙抢读锁会饿死写者（shared_mutex 无写优先保证）
      for (int i = 0; i < kIters * 2 && !stop.load(std::memory_order_relaxed); ++i) {
        std::this_thread::yield();
        // ttl=now：floor=0，写入的时间戳（1~500）全部 > 0 → 读者总能看到全部条目，
        // 去重/上界两个不变量才非空转
        auto workers = reg.HealthyWorkers(now, /*ttl=*/now);
        std::set<std::string> seen;
        for (const auto& w : workers) {
          EXPECT_TRUE(seen.insert(w.endpoint).second) << "duplicate: " << w.endpoint;
          EXPECT_LE(w.last_heartbeat_ms, now);
          // 快照自洽：每个字段都必须落在某次真实 upsert 的值域内（endpoint 归属某个
          // 写者网段、hb 在合法迭代范围内）。string 按值存储下撕裂读结构上不可能，
          // 但若未来引入 lock-free 读路径（shared_ptr 快照），这条断言就是第一道闸。
          const int subnet = std::stoi(w.endpoint.substr(7));  // "10.0.0." 之后（索引 7 起是子网数字）
          EXPECT_GE(subnet, 0);
          EXPECT_LT(subnet, kWriters);
          EXPECT_GT(w.last_heartbeat_ms, 0);
          EXPECT_LE(w.last_heartbeat_ms, kIters);
        }
      }
    });
  }
  for (int w = 0; w < kWriters; ++w) threads[w].join();
  stop.store(true);
  for (int r = kWriters; r < kWriters + kReaders; ++r) threads[r].join();

  // 终态不变量：170*8 个唯一 endpoint 全部在册
  EXPECT_EQ(reg.HealthyWorkers(/*now=*/1000000, /*ttl=*/1000000).size(),
            static_cast<size_t>(kWriters * 170));
}

}  // namespace
