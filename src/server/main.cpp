#include <butil/logging.h>  // brpc 内嵌日志：与系统 glog 互斥（双 -v 注册）
#include <gflags/gflags.h>

#include <cstdlib>

#include "compress/infra.h"
#include "compress/options.h"
#include "server_runtime.h"

// 默认值单源：引用 Options 字段，避免 DEFINE 与结构体双写漂移（master main 同构）
DEFINE_uint32(port, 8002, "server 监听端口");
DEFINE_string(master_addr, compress::Options{}.master_addr, "master 地址");
DEFINE_int32(server_worker_count, compress::Options{}.server_worker_count, "压缩 worker 线程数");
DEFINE_int32(server_queue_capacity, compress::Options{}.server_queue_capacity,
             "压缩任务有界队列容量（容量+线程=系统容量上限）");

int main(int argc, char* argv[]) {
  compress::InitFlags(&argc, &argv);  // glog-free 启动（glog 与 butil 日志互斥，见 infra.h 注释）
  // 信号处置先于网络服务：安装前 TERM 落下是内核默认处置（直接杀，exit 143），
  // 优雅退出语义失效——"先装门铃，再开门营业"
  compress::InstallShutdownHandlers();

  compress::Options opts;
  opts.master_addr = FLAGS_master_addr;
  opts.server_worker_count = FLAGS_server_worker_count;
  opts.server_queue_capacity = FLAGS_server_queue_capacity;

  compress::ServerRuntime rt(opts);
  if (rt.Start(static_cast<int>(FLAGS_port)) < 0) {
    return EXIT_FAILURE;
  }

  const int sig = compress::WaitForShutdownSignal();
  LOG(INFO) << "compress_server received signal " << sig << ", stopping";
  rt.Stop();
  LOG(INFO) << "compress_server stopped";
  return EXIT_SUCCESS;
}
