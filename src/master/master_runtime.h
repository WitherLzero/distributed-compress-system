// master_runtime.h —— master 进程装配线：Options + Clock → registry / services / scanner / server。
// 生产 main 与测试走同一入口，保证"测的 = 产的"（测试与 main.cpp 同构）。
#pragma once

#include <brpc/server.h>

#include "compress/options.h"
#include "death_scanner.h"
#include "master_service_impl.h"
#include "server_registry.h"

namespace compress {

class MasterRuntime {
 public:
  // clock 传空 = 真实时钟（steady_clock 毫秒）；测试注入假时钟。
  explicit MasterRuntime(Options options, Clock clock = {});
  ~MasterRuntime();  // 兜底 Stop()

  MasterRuntime(const MasterRuntime&) = delete;
  MasterRuntime& operator=(const MasterRuntime&) = delete;

  // 起 BRPC 服务与判死扫描。port=0 由内核分配（测试）。
  // 返回实际监听端口；失败返回 -1（错误经 butil 日志输出，master/server 进程 glog-free）。
  int Start(int port);

  // 优雅退出：先停扫描线程（不再动注册表），再停 BRPC（不再来新请求）。
  void Stop();

  ServerRegistry& registry() { return registry_; }
  int listen_port() const { return port_; }

 private:
  Options options_;
  ServerRegistry registry_;
  RegistryServiceImpl registry_svc_;  // 引用成员：初始化序即声明序，registry_ 在前
  MasterServiceImpl master_svc_;
  DeathScanner scanner_;
  brpc::Server server_;
  int port_ = -1;
};

}  // namespace compress
