// master_service_impl.h —— MasterService + RegistryService 的 BRPC 实现。
// RegistryService：首报心跳即注册（Upsert）；MasterService：查询原样返回注册表全量
// （不排序、不二次过滤——选点由 client 随机自选，§3.2 职责划分）。
// 时间戳由 master 侧 Clock 统一取值（事件时间不信任调用方）。
#pragma once

#include <chrono>

#include "compress_service.pb.h"
#include "server_registry.h"

namespace compress {

class MasterServiceImpl : public system::MasterService {
 public:
  MasterServiceImpl(ServerRegistry& registry, Clock clock,
                    std::chrono::milliseconds ttl_ms);
  void GetWorkersInfo(google::protobuf::RpcController* cntl,
                      const system::WorkersInfoRequest* req,
                      system::WorkersInfoResponse* resp,
                      google::protobuf::Closure* done) override;

 private:
  ServerRegistry& registry_;
  Clock clock_;
  std::chrono::milliseconds ttl_;
};

class RegistryServiceImpl : public system::RegistryService {
 public:
  RegistryServiceImpl(ServerRegistry& registry, Clock clock,
                      std::chrono::milliseconds beat_interval_ms);
  void Heartbeat(google::protobuf::RpcController* cntl,
                 const system::HeartbeatRequest* req,
                 system::HeartbeatResponse* resp,
                 google::protobuf::Closure* done) override;

 private:
  ServerRegistry& registry_;
  Clock clock_;
  std::chrono::milliseconds beat_interval_;
};

}  // namespace compress
