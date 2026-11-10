#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>

#include "compress/infra.h"

DEFINE_string(master_addr, "127.0.0.1:8001", "master 地址");
DEFINE_uint32(port, 8002, "server 监听端口");

int main(int argc, char* argv[]) {
  compress::InitInfra(&argc, &argv);
  LOG(INFO) << "compress_server starting, port=" << FLAGS_port << " master=" << FLAGS_master_addr;

  const int sig = compress::WaitForShutdownSignal();

  LOG(INFO) << "compress_server stopped, signal=" << sig;
  return EXIT_SUCCESS;
}
