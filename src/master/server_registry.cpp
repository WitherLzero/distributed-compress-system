#include "server_registry.h"

namespace compress {

// 锁纪律：shared_mutex 只护 map 本体；HealthyWorkers 的返回 vector 在读锁内构造——
// shared_lock 下读者互不排斥，写者（心跳 upsert）不被读者长时间阻塞（§3.2 被动组件，
// 心跳路径微秒级）。阈值（now/ttl）永远来自参数，注册表不持有策略状态。

void ServerRegistry::Upsert(const WorkerInfo& w) {
  {
    std::unique_lock<std::shared_mutex> lk(mtx_);
    workers_[w.endpoint] = w;  // 同址心跳 = 刷新时间戳，不产生重复条目
    alive_size_.set_value(static_cast<std::int64_t>(workers_.size()));
  }
  upsert_total_ << 1;
}

std::vector<WorkerInfo> ServerRegistry::HealthyWorkers(std::int64_t now_ms,
                                                       std::int64_t ttl_ms) {
  const std::int64_t floor = now_ms - ttl_ms;  // hb <= floor 判死（严格：hb > floor 才活）
  std::shared_lock<std::shared_mutex> lk(mtx_);
  std::vector<WorkerInfo> out;
  out.reserve(workers_.size());
  for (const auto& [ep, w] : workers_) {
    if (w.last_heartbeat_ms > floor) {
      out.push_back(w);
    }
  }
  return out;
}

void ServerRegistry::RemoveExpired(std::int64_t now_ms, std::int64_t ttl_ms) {
  const std::int64_t floor = now_ms - ttl_ms;
  std::unique_lock<std::shared_mutex> lk(mtx_);
  for (auto it = workers_.begin(); it != workers_.end();) {
    if (it->second.last_heartbeat_ms <= floor) {
      it = workers_.erase(it);
    } else {
      ++it;
    }
  }
  alive_size_.set_value(static_cast<std::int64_t>(workers_.size()));
}

}  // namespace compress
