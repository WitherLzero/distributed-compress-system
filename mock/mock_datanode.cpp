#include <cstdlib>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "compress/infra.h"

DEFINE_uint32(port, 8003, "mock_datanode 监听端口（联调期 ReadBlock 服务，见 03 号 issue）");

int main(int argc, char* argv[]) {
    compress::InitInfra(&argc, &argv);
    LOG(INFO) << "mock_datanode starting, port=" << FLAGS_port;

    const int sig = compress::WaitForShutdownSignal();

    LOG(INFO) << "mock_datanode stopped, signal=" << sig;
    return EXIT_SUCCESS;
}
