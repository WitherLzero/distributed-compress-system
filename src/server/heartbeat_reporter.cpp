// compress_server 侧（票 06 / CP5）：心跳客户端实现。
// 周期合同：master 响应 beat_interval_ms 裁定下一跳周期（master 配置单源），
// 心跳失败期间按 fallback 自转——server 不能因 master 抖动而停跳。
#include "heartbeat_reporter.h"

#include <butil/logging.h>

namespace compress {

HeartbeatReporter::HeartbeatReporter(std::string master_addr, std::string server_id,
                                     int listen_port, std::function<LoadSample()> load,
                                     std::chrono::milliseconds fallback_interval_ms)
    : master_addr_(std::move(master_addr)),
      server_id_(std::move(server_id)),
      listen_port_(listen_port),
      load_(std::move(load)),
      fallback_interval_ms_(fallback_interval_ms) {}

void HeartbeatReporter::Start() {
  channel_ready_ = channel_.Init(master_addr_.c_str(), nullptr) == 0;
  if (!channel_ready_) {
    LOG(WARNING) << "heartbeat channel init failed for " << master_addr_;
  }
  thread_ = std::thread([this] { Loop(); });
}

void HeartbeatReporter::Stop() noexcept {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stop_.load()) return;  // 幂等
    stop_.store(true);
  }
  cv_.notify_all();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void HeartbeatReporter::Loop() {
  if (!channel_ready_) {
    // 通道不可用：不能注册，线程无事可做，等 Stop 收尸。
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return stop_.load(); });
    return;
  }
  for (;;) {
    // 慢事前置（与 DeathScanner::Loop 同构）：心跳在锁外，产物 interval
    // 喂给下一行 wait_for——锁的寿命 = 睡眠那一段，Stop 期间随时可插进来。
    const auto interval = BeatOnce();
    std::unique_lock<std::mutex> lock(mu_);
    if (cv_.wait_for(lock, interval, [this] { return stop_.load(); })) {
      return;  // 被 Stop 叫醒：谓词为真
    }
  }
}


std::chrono::milliseconds HeartbeatReporter::BeatOnce() {
  system::RegistryService_Stub stub(&channel_);
  brpc::Controller cntl;
  cntl.set_timeout_ms(kRpcTimeoutMs);  // master 半死时本跳失败封顶，进 fallback 自转
  system::HeartbeatRequest req;
  req.set_server_id(server_id_);
  req.set_port(listen_port_);
  if (load_) {
    const LoadSample s = load_();
    req.set_cpu_percent(static_cast<std::uint32_t>(s.cpu_percent));
    req.set_mem_percent(static_cast<std::uint32_t>(s.mem_percent));
    req.set_queue_len(s.queue_len);
  }
  system::HeartbeatResponse resp;
  stub.Heartbeat(&cntl, &req, &resp, nullptr);  // 同步心跳（done=nullptr）
  if (cntl.Failed()) {
    LOG(WARNING) << "heartbeat to " << master_addr_ << " failed: " << cntl.ErrorText();
    return fallback_interval_ms_;  // master 不可达：按兜底周期自转
  }
  if (resp.beat_interval_ms() > 0) {
    return std::chrono::milliseconds(resp.beat_interval_ms());
  }
  return fallback_interval_ms_;
}

}  // namespace compress
