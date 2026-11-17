#include <brpc/server.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>

#include "compress/infra.h"
#include "master_service_impl.h"

DEFINE_uint32(port, 8001, "master 监听端口");

int main(int argc, char* argv[]) {
  compress::InitInfra(&argc, &argv);

  compress::RegistryServiceImpl registry_svc;
  compress::MasterServiceImpl master_svc;

  brpc::Server server;
  if (server.AddService(&registry_svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "AddService RegistryService failed";
    return EXIT_FAILURE;
  }
  if (server.AddService(&master_svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "AddService MasterService failed";
    return EXIT_FAILURE;
  }

  brpc::ServerOptions opts;
  opts.idle_timeout_sec = -1;
  if (server.Start(FLAGS_port, &opts) != 0) {
    LOG(ERROR) << "master Start failed, port=" << FLAGS_port;
    return EXIT_FAILURE;
  }

  LOG(INFO) << "compress_master listening on port " << FLAGS_port;
  server.RunUntilAskedToQuit();

  LOG(INFO) << "compress_master stopped";
  return EXIT_SUCCESS;
}
