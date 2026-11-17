// master_service_impl.h —— CP0 骨架：MasterService + RegistryService 的最薄 BRPC 实现。
// Heartbeat 固定返回 accepted=true，GetWorkersInfo 返回空表。
// 业务逻辑（注册表、判死）随 CP1/CP2 填入；CP0 只验证通信链路。
#pragma once

#include "compress_service.pb.h"

namespace compress {

class MasterServiceImpl : public system::MasterService {
 public:
  void GetWorkersInfo(google::protobuf::RpcController* cntl,
                      const system::WorkersInfoRequest* req,
                      system::WorkersInfoResponse* resp,
                      google::protobuf::Closure* done) override;
};

class RegistryServiceImpl : public system::RegistryService {
 public:
  void Heartbeat(google::protobuf::RpcController* cntl,
                 const system::HeartbeatRequest* req,
                 system::HeartbeatResponse* resp,
                 google::protobuf::Closure* done) override;
};

}  // namespace compress
