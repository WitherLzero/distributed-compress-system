#include "master_service_impl.h"

#include <butil/logging.h>  // brpc 内嵌日志：与系统 glog 互斥（双 -v 注册）
#include <brpc/controller.h>

namespace compress {

namespace {

// 心跳连接的来源地址（观测用）；实例自身服务端口以请求字段为准。
std::string RemoteIp(brpc::Controller* cntl) {
  return butil::ip2str(cntl->remote_side().ip).c_str();
}

}  // namespace

MasterServiceImpl::MasterServiceImpl(ServerRegistry& registry, Clock clock,
                                     std::chrono::milliseconds ttl_ms)
    : registry_(registry), clock_(std::move(clock)), ttl_(ttl_ms) {}

void MasterServiceImpl::GetWorkersInfo(
    google::protobuf::RpcController* /*cntl*/,
    const system::WorkersInfoRequest* /*req*/,
    system::WorkersInfoResponse* resp,
    google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  const std::int64_t now = clock_();
  // 原样返回当前存活全量：不排序、不指定、不二次过滤（选点在 client 侧）
  for (const auto& w : registry_.HealthyWorkers(now, ttl_.count())) {
    system::WorkerInfo* out = resp->add_workers();
    out->set_server_id(w.endpoint);
    out->set_ip(w.ip);
    out->set_port(w.port);
    out->set_cpu_percent(w.cpu_percent);
    out->set_mem_percent(w.mem_percent);
    out->set_queue_len(w.queue_len);
    out->set_last_hb_ms(w.last_heartbeat_ms);
  }
}

RegistryServiceImpl::RegistryServiceImpl(ServerRegistry& registry, Clock clock,
                                         std::chrono::milliseconds beat_interval_ms)
    : registry_(registry), clock_(std::move(clock)), beat_interval_(beat_interval_ms) {}

void RegistryServiceImpl::Heartbeat(
    google::protobuf::RpcController* raw_cntl,
    const system::HeartbeatRequest* req,
    system::HeartbeatResponse* resp,
    google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto* cntl = static_cast<brpc::Controller*>(raw_cntl);

  WorkerInfo w;
  w.endpoint = req->server_id();
  w.ip = RemoteIp(cntl);
  w.port = req->port();
  w.cpu_percent = req->cpu_percent();
  w.mem_percent = req->mem_percent();
  w.queue_len = req->queue_len();
  w.last_heartbeat_ms = clock_();  // 事件时间由 master 统一裁定
  registry_.Upsert(w);

  resp->set_accepted(true);
  resp->set_beat_interval_ms(static_cast<std::int32_t>(beat_interval_.count()));
  LOG(INFO) << "heartbeat from " << w.endpoint << " (" << w.ip << ":" << w.port
            << ") queue_len=" << w.queue_len;
}

}  // namespace compress
