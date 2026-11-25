#include "master_runtime.h"

#include <butil/logging.h>  // brpc 内嵌日志：与系统 glog 互斥（双 -v 注册），链接 BRPC 的二进制统一走这里
#include <brpc/controller.h>
#include <chrono>

namespace compress {

namespace {

Clock RealClockMs() {
  return [] {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  };
}

}  // namespace

MasterRuntime::MasterRuntime(Options options, Clock clock)
    : options_(std::move(options)),
      registry_svc_(registry_, clock ? clock : RealClockMs(),
                    std::chrono::seconds(options_.heartbeat_interval_sec)),
      master_svc_(registry_, clock ? clock : RealClockMs(),
                  std::chrono::seconds(options_.death_ttl_sec)),
      scanner_(registry_, clock ? clock : RealClockMs(),
               std::chrono::seconds(options_.death_ttl_sec),
               std::chrono::seconds(options_.death_scan_interval_sec)) {}

MasterRuntime::~MasterRuntime() { Stop(); }

int MasterRuntime::Start(int port) {
  if (server_.AddService(&registry_svc_, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "AddService RegistryService failed";
    return -1;
  }
  // 慢 handler 并发上限（写者饿死解法阶梯第一层：入口限并发，读者数有上界）。
  // 算账：心跳 300 实例 × 5s 周期 = 60 QPS，256 上限 ≈ 4 倍冗余；查询由 client
  // 周期拉取（§3.2），64 上界防慢 handler 并发打爆。语义是熔断阀门不是容量规划，
  // 联调期按 storm 格实测标定（与 §3.3.4 内存预算同款处理）。
  if (server_.AddService(&master_svc_, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "AddService MasterService failed";
    return -1;
  }

  // 时序约束（brpc 双重契约）：方法须已 AddService（否则查无此方法），
  // 且必须在 Start 前调用（server.h:615，Start 后设置静默失效）。
  server_.MaxConcurrencyOf("compress.system.MasterService.GetWorkersInfo") = 64;
  server_.MaxConcurrencyOf("compress.system.RegistryService.Heartbeat") = 256;

  brpc::ServerOptions opts;
  opts.idle_timeout_sec = -1;
  if (server_.Start(port, &opts) != 0) {
    LOG(ERROR) << "master Start failed, port=" << port;
    return -1;
  }
  port_ = server_.listen_address().port;

  scanner_.Start();
  LOG(INFO) << "master listening on port " << port_
            << " ttl=" << options_.death_ttl_sec << "s"
            << " scan=" << options_.death_scan_interval_sec << "s";
  return port_;
}

void MasterRuntime::Stop() {
  scanner_.Stop();  // 先停扫描（其对 registry 的访问随线程退出终结）
  server_.Stop(0);  // 再停 BRPC：拒绝新请求，等在途请求收尾
  server_.Join();
}

}  // namespace compress
