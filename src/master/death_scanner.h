// death_scanner.h —— 判死扫描线程：周期遍历注册表时间戳并物理摘除过期条目。
// 时机策略层：事实谓词在 ServerRegistry::RemoveExpired；纯本地操作，不发 RPC（§7.2）。
// 判死阈值 = 心跳间隔 × 倍数（当前 4×5s=20s），扫描周期 1~2s 远小于阈值（§3.2）。
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include "server_registry.h"

namespace compress {

class DeathScanner {
 public:
  DeathScanner(ServerRegistry& registry, Clock clock,
               std::chrono::milliseconds ttl_ms,
               std::chrono::milliseconds scan_interval_ms);
  ~DeathScanner();  // RAII：析构兜底 Stop()

  DeathScanner(const DeathScanner&) = delete;
  DeathScanner& operator=(const DeathScanner&) = delete;

  void Start();  // 起扫描线程进入循环
  void Stop();   // 唤醒等待并 join；等待中调用立即返回

 private:
  // 手动触发一轮清扫：Loop 专用，不对外暴露——运行中无合法的手动清扫调用方
  // （启动清扫归宿主进程一次性执行；查询路径本身惰性过滤，不依赖摘除时机）。
  void RunOnce();

  void Loop();

  ServerRegistry& registry_;
  Clock clock_;
  std::chrono::milliseconds ttl_;
  std::chrono::milliseconds interval_;

  std::thread thread_;
  std::atomic<bool> stop_{false};
  // 只为可中断等待服务：wait_for(interval) 中 Stop 需要 notify 立即醒来。
  std::mutex wake_mtx_;
  std::condition_variable wake_;
};

}  // namespace compress
