// server_registry.h —— Master 实例注册表。内存态，不持久化（设计文档 §3.2）。
// 查询按 TTL 过滤判死"事实"；摘除时机归 DeathScanner（CP2）。被动组件：不发探测 RPC。
#pragma once

#include <bvar/bvar.h>

#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace compress {

// 事件时间源（毫秒）。生产传真实时钟；测试传假时钟——时间即测试输入数据。
// 统一在此别名（registry/scanner/service 三处共用同一类型）。
using Clock = std::function<std::int64_t()>;

struct WorkerInfo {
  std::string endpoint;                // 实例标识（server_id）；注册 = 首次心跳 upsert
  std::string ip;                      // 心跳连接的来源地址（remote_side），观测用
  std::int32_t port = 0;               // 实例自身服务端口（心跳上报）
  std::uint32_t cpu_percent = 0;       // 负载字段：随心跳入表、随列表下发（策略演进备用）
  std::uint32_t mem_percent = 0;
  std::uint32_t queue_len = 0;
  std::int64_t last_heartbeat_ms = 0;  // 最近心跳时间戳：判死的唯一依据
};

class ServerRegistry {
 public:
  ServerRegistry() = default;

  ServerRegistry(const ServerRegistry&) = delete;
  ServerRegistry& operator=(const ServerRegistry&) = delete;

  // 注册/续期：按 endpoint upsert，同址心跳刷新时间戳不产生重复条目。
  // WorkerInfo 的 last_heartbeat_ms 由调用方（心跳 handler）填当前时间。
  void Upsert(const WorkerInfo& w);

  // 判死"事实"谓词：返回 last_heartbeat_ms > now - ttl 的全部条目（无序）。
  // 惰性语义：过期条目不一定立刻删除，只保证不出现在返回值里。
  std::vector<WorkerInfo> HealthyWorkers(std::int64_t now_ms, std::int64_t ttl_ms);

  // 显式清扫：物理删除过期条目。DeathScanner（CP2）周期调用。
  void RemoveExpired(std::int64_t now_ms, std::int64_t ttl_ms);

 private:
  mutable std::shared_mutex mtx_;
  std::unordered_map<std::string, WorkerInfo> workers_;
  // /vars 导出：风暴观测曲线的数据源（issue 07 storm 格）
  // 条目数是"当前值"不是"累加值"，用 Status 在每个变更点 set_value；
  // upsert_total_ 是真累加量，用 Adder。名字带 compress_master_ 前缀便于 /vars 过滤。
  bvar::Status<std::int64_t> alive_size_{"compress_master_registry_size", 0};
  bvar::Adder<std::int64_t> upsert_total_{"compress_master_upsert_total"};
};

}  // namespace compress
