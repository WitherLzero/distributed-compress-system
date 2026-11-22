#include "compress/infra.h"

#include <gflags/gflags.h>

#include <chrono>
#include <csignal>
#include <thread>

namespace compress {

void InitFlags(int* argc, char*** argv) {
  gflags::ParseCommandLineFlags(argc, argv, true);
}

namespace {

volatile std::sig_atomic_t g_stop_signal = 0;

void HandleStopSignal(int sig) {
  g_stop_signal = sig;
}

}  // namespace

void InstallShutdownHandlers() {
  std::signal(SIGINT, HandleStopSignal);
  std::signal(SIGTERM, HandleStopSignal);
}

int WaitForShutdownSignal() {
  InstallShutdownHandlers();  // 幂等兜底：未提前安装的调用方仍安全
  while (g_stop_signal == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }
  return g_stop_signal;
}

}  // namespace compress
