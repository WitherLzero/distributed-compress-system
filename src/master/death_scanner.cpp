#include "death_scanner.h"

namespace compress {

DeathScanner::DeathScanner(ServerRegistry& registry, Clock clock,
                           std::chrono::milliseconds ttl_ms,
                           std::chrono::milliseconds scan_interval_ms)
    : registry_(registry), clock_(std::move(clock)),
      ttl_(ttl_ms), interval_(scan_interval_ms) {}

DeathScanner::~DeathScanner() { Stop(); }

void DeathScanner::Start() {
  if (thread_.joinable()) return;  // 已在跑
  thread_ = std::thread([this] { Loop(); });
}

void DeathScanner::Stop() {
  {
    // 置位必须持锁：wait_for 在持锁状态下检查谓词后才挂起；若置位不持锁，
    // 它可能落在"谓词已查完（false）、线程尚未挂起"的窗口内，notify 随之丢失
    //（lost wakeup），扫描线程将继续睡满整个 interval 后才返回。
    std::lock_guard<std::mutex> lk(wake_mtx_);
    stop_.store(true);  // 持锁内 relaxed 亦可，但默认序免解释（语义此处无需放松）
  }
  // 当前设计只有扫描线程一个等待者，notify_one 足够且略省开销；
  // 若将来加入第二个等待者（如停机时的观测线程），须改回 notify_all。
  wake_.notify_one();
  if (thread_.joinable()) thread_.join();
}

void DeathScanner::RunOnce() { registry_.RemoveExpired(clock_(), ttl_.count()); }

void DeathScanner::Loop() {
  while (!stop_.load()) {
    RunOnce();  // 扫描不持 wake_mtx_：锁只守等待，Stop 不必等整轮扫完
    // 可中断睡眠：不等满一个 interval，Stop 的 notify 立刻唤醒。
    // wait_for 在持锁状态下检查谓词并入睡，Stop 的置位也持锁——无丢唤醒窗口。
    std::unique_lock<std::mutex> lk(wake_mtx_);
    wake_.wait_for(lk, interval_,
                   [this] { return stop_.load(); });
  }
}

}  // namespace compress
