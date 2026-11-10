#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>

#include "compress/infra.h"

DEFINE_uint32(port, 8001, "master 监听端口");

int main(int argc, char* argv[]) {
  compress::InitInfra(&argc, &argv);
  LOG(INFO) << "compress_master starting, port=" << FLAGS_port;

  const int sig = compress::WaitForShutdownSignal();

  LOG(INFO) << "compress_master stopped, signal=" << sig;
  return EXIT_SUCCESS;
}
