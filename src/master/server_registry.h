// compress_master 侧骨架（票 08）：实例注册表。内存态，不持久化。
// 查询原样返回注册表全量，不二次过滤——失效剔除是 DeathScanner 的职责（设计文档 §3.2.3）。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace compress {

struct WorkerInfo {
  std::string endpoint;                // 实例地址；注册 = 首次心跳 upsert
  std::int64_t last_heartbeat_ms = 0;  // 最近心跳时间戳：DeathScanner 判死的唯一依据
  // 负载字段（queue_len/cpu/mem，心跳上报 §6.1）随 W4 补充
};

class ServerRegistry {
 public:
  ServerRegistry() = default;

  void Upsert(const WorkerInfo& w);          // 注册 = 首次心跳 upsert
  std::vector<WorkerInfo> HealthyWorkers();  // 原样返回当前注册表全量
};

}  // namespace compress
