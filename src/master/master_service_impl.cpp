#include "master_service_impl.h"

#include <brpc/controller.h>
#include <glog/logging.h>

namespace compress {

void MasterServiceImpl::GetWorkersInfo(
    google::protobuf::RpcController* /*cntl*/,
    const system::WorkersInfoRequest* /*req*/,
    system::WorkersInfoResponse* /*resp*/,
    google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  // CP0：空表，不注册任何 worker
}

void RegistryServiceImpl::Heartbeat(
    google::protobuf::RpcController* /*cntl*/,
    const system::HeartbeatRequest* req,
    system::HeartbeatResponse* resp,
    google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  resp->set_accepted(true);
  LOG(INFO) << "heartbeat from " << req->server_id() << ":" << req->port();
}

}  // namespace compress
