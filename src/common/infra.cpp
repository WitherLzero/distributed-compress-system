#include "compress/infra.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <chrono>
#include <csignal>
#include <thread>

namespace compress {

void InitInfra(int* argc, char*** argv) {
  gflags::ParseCommandLineFlags(argc, argv, true);
  google::InitGoogleLogging((*argv)[0]);
  google::InstallFailureSignalHandler();  // 崩溃栈追踪，联调期定位用
  FLAGS_logtostderr = true;               // 开发期日志直出 stderr
}

namespace {

volatile std::sig_atomic_t g_stop_signal = 0;

void HandleStopSignal(int sig) {
  g_stop_signal = sig;
}

}  // namespace

int WaitForShutdownSignal() {
  std::signal(SIGINT, HandleStopSignal);
  std::signal(SIGTERM, HandleStopSignal);
  while (g_stop_signal == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }
  return g_stop_signal;
}

}  // namespace compress
