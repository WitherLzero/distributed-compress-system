// compress_server 组合根（票 06 / CP6）：把 WorkerPool + CompressServiceImpl +
// brpc::Server + HeartbeatReporter 装配成可运行的 server 运行时。
// 成员声明序 = 生死簿：依赖者在前、被依赖者在后，析构天然逆序（CP3 同构）。
// 启动顺序：起 brpc::Server → 起心跳（首报即注册）。
// server 是无状态计算进程，不落盘、无 .tmp 半成品——temp 清扫是 datanode（client）
// 侧职责（§3.1.4），不在本模块。
// 停机顺序（票 06 验收第 6 条）：先停收新请求（brpc::Server::Stop），
// 再终止在途任务并反馈失败（WorkerPool::Stop 排干+FailTask），最后停心跳线程。
#pragma once

#include <memory>

#include <brpc/server.h>

#include "compress/options.h"
#include "compress_service_impl.h"
#include "heartbeat_reporter.h"
#include "load_sampler.h"
#include "worker_pool.h"

namespace compress {

class ServerRuntime {
 public:
  explicit ServerRuntime(Options options);
  ~ServerRuntime();  // 兜底 Stop：调用方漏停时对象析构不挂死

  ServerRuntime(const ServerRuntime&) = delete;
  ServerRuntime& operator=(const ServerRuntime&) = delete;

  /// 起全部组件；成功返回监听端口（传 0 由内核分配），失败返回 -1。
  int Start(int port);

  /// 幂等停机：brpc 停收 → 池排干 → 心跳停。
  void Stop() noexcept;

  int listen_port() const { return listen_port_; }
  const std::string& server_id() const { return server_id_; }

 private:
  // 心跳 reporter 需要真实监听端口（port=0 时内核分配后才知），故 Start 内构造。
  // 声明在最后：即便调用方未 Stop 直接析构，reporter 也最先死（它不依赖池）。
  Options options_;
  LoadSampler sampler_;  // 心跳负载取样（CPU 差分有状态：prev 计数器随对象存活）
  WorkerPool pool_;
  CompressServiceImpl compress_svc_;
  brpc::Server server_;
  std::unique_ptr<HeartbeatReporter> reporter_;
  int listen_port_ = -1;
  std::string server_id_;
};

}  // namespace compress
