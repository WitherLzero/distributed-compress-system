// compress_server 侧（票 06 / CP5）：心跳客户端线程。
// 启动即发首跳（首报即注册），此后按 master 响应的 beat_interval_ms 周期上报
// queue_len/cpu/mem（§7.2：周期合同由 master 配置单源裁定）。
// Stop 走带谓词 wait_for 双醒路径：定时醒（到点发下一跳）或提前醒（退出）。
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <brpc/channel.h>

#include "compress_service.pb.h"

namespace compress {

// 一跳的负载快照：取样与上报间隔 = 心跳周期，消费方拿到的数据新鲜度 ≤ 周期。
// 该语义与设计 §3.2 一致——master 只透传不二次过滤，陈旧度由消费方（W5 选点）自担。
struct LoadSample {
  double cpu_percent;
  double mem_percent;
  std::uint32_t queue_len;
};

// 心跳 RPC 超时（毫秒）：master 半死（TCP 挂起）时本跳失败封顶，
// 否则心跳线程可卡在一次 RPC 里很久，Stop 也要跟着等。
constexpr int kRpcTimeoutMs = 2000;

class HeartbeatReporter {
 public:
  /// load：负载取样回调，每跳调用一次（组合根负责把 WorkerPool/系统指标接进来）。
  /// fallback_interval_ms：心跳失败时的兜底周期（master 不可达期间自转）。
  HeartbeatReporter(std::string master_addr, std::string server_id, int listen_port,
                    std::function<LoadSample()> load,
                    std::chrono::milliseconds fallback_interval_ms =
                        std::chrono::milliseconds(5000));
  ~HeartbeatReporter() { Stop(); }

  HeartbeatReporter(const HeartbeatReporter&) = delete;
  HeartbeatReporter& operator=(const HeartbeatReporter&) = delete;

  /// 起线程并立即发首跳（同步于调用线程，失败仅记录不致命——尽力而为路径）。
  void Start();

  /// 幂等停机：置 stop → 唤醒等待中的心跳线程 → join。
  void Stop() noexcept;

 private:
  void Loop();
  /// 发一跳，返回 master 裁定的下一跳周期；失败返回 fallback。
  std::chrono::milliseconds BeatOnce();

  const std::string master_addr_;
  const std::string server_id_;
  const int listen_port_;
  std::function<LoadSample()> load_;
  const std::chrono::milliseconds fallback_interval_ms_;

  brpc::Channel channel_;
  bool channel_ready_ = false;

  std::mutex mu_;
  std::condition_variable cv_;
  std::atomic<bool> stop_{false};  // 循环条件锁外可见 → atomic（stop 三形态第二形态）
  std::thread thread_;
};

}  // namespace compress
