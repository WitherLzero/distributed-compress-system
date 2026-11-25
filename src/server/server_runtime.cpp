#include "server_runtime.h"

#include <unistd.h>

#include <algorithm>
#include <thread>
#include <utility>

#include <butil/logging.h>

namespace compress {

ServerRuntime::ServerRuntime(Options options)
    : options_(std::move(options)),
      // worker_count 0 = auto：按硬件并发（Options 惯例）。压缩是 CPU 密集，
      // worker 数即 CPU 并发上限——静态默认在 4 核机浪费、64 核机饿死。
      pool_(options_.server_worker_count > 0
                ? static_cast<std::size_t>(options_.server_worker_count)
                : static_cast<std::size_t>(
                      std::max(1u, std::thread::hardware_concurrency())),
            static_cast<std::size_t>(options_.server_queue_capacity)),
      compress_svc_(pool_) {}

ServerRuntime::~ServerRuntime() { Stop(); }

int ServerRuntime::Start(int port) {
  if (listen_port_ > 0) {
    return listen_port_;  // 幂等：已启动直接返回
  }
  if (server_.AddService(&compress_svc_, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "add CompressService failed";
    return -1;
  }
  if (server_.Start(port, nullptr) != 0) {
    LOG(ERROR) << "compress_server start failed on port " << port;
    return -1;
  }
  listen_port_ = server_.listen_address().port;
  server_id_ = "srv-" + std::to_string(static_cast<long long>(::getpid())) + "-" +
               std::to_string(listen_port_);

  reporter_ = std::make_unique<HeartbeatReporter>(
      options_.master_addr, server_id_, listen_port_, [this] {
        // 快照契约：上报值反映"本跳发出时刻"的状态，master 侧消费方的数据新鲜度
        // ≤ 心跳周期（beat_interval_ms，master 裁定）。CPU 为相邻心跳间隔的系统级
        // 利用率增量（间隔变算法自动正确），MEM 为系统已用占比（节点余量视角）。
        return LoadSample{sampler_.CpuUtilization(), sampler_.MemUtilization(),
                          static_cast<std::uint32_t>(pool_.PendingTasks())};
      });
  reporter_->Start();  // 首报即注册：此刻 master 注册表里出现本实例

  LOG(INFO) << "compress_server starting: port=" << listen_port_ << " id=" << server_id_
            << " master=" << options_.master_addr
            << " workers=" << options_.server_worker_count
            << " queue_capacity=" << options_.server_queue_capacity;
  return listen_port_;
}

void ServerRuntime::Stop() noexcept {
  if (listen_port_ < 0) {
    return;  // 未启动：no-op（幂等语义）
  }
  // 先停收新请求；在途任务由池排干并以 FAILED 业务响应收尾（连接可能已关，
  // done->Run 仍必须执行——client 至多收到 EOF，绝不悬挂）。
  server_.Stop(0);
  pool_.Stop();
  if (reporter_) {
    reporter_->Stop();
  }
  listen_port_ = -1;
  LOG(INFO) << "compress_server stopped (id=" << server_id_ << ")";
}

}  // namespace compress
