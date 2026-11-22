#include <butil/logging.h>  // brpc 内嵌日志：与系统 glog 互斥（双 -v 注册）
#include <gflags/gflags.h>

#include <cstdlib>

#include "compress/infra.h"
#include "master_runtime.h"

// 默认值单源：引用 Options 字段，避免 DEFINE 与结构体双写漂移
DEFINE_uint32(port, 8001, "master 监听端口");
DEFINE_int32(heartbeat_interval_sec, compress::Options{}.heartbeat_interval_sec,
             "server 心跳周期（响应 beat_interval_ms）");
DEFINE_int32(death_ttl_sec, compress::Options{}.death_ttl_sec, "判死阈值 = 心跳间隔 × 4");
DEFINE_int32(death_scan_interval_sec, compress::Options{}.death_scan_interval_sec,
             "判死扫描周期");

int main(int argc, char* argv[]) {
  compress::InitFlags(&argc, &argv);  // glog-free 启动（glog 与 butil 日志互斥，见 infra.h 注释）
  // 信号处置先于网络服务：安装前 TERM 落下是内核默认处置（直接杀，exit 143），
  // 优雅退出语义失效——"先装门铃，再开门营业"
  compress::InstallShutdownHandlers();

  compress::Options opts;
  opts.heartbeat_interval_sec = FLAGS_heartbeat_interval_sec;
  opts.death_ttl_sec = FLAGS_death_ttl_sec;
  opts.death_scan_interval_sec = FLAGS_death_scan_interval_sec;

  compress::MasterRuntime rt(opts);
  LOG(INFO) << "compress_master starting: port=" << FLAGS_port
            << " heartbeat=" << FLAGS_heartbeat_interval_sec << "s"
            << " ttl=" << FLAGS_death_ttl_sec << "s"
            << " scan=" << FLAGS_death_scan_interval_sec << "s";
  if (rt.Start(static_cast<int>(FLAGS_port)) < 0) {
    return EXIT_FAILURE;
  }

  const int sig = compress::WaitForShutdownSignal();
  LOG(INFO) << "compress_master received signal " << sig << ", stopping";
  rt.Stop();
  LOG(INFO) << "compress_master stopped";
  return EXIT_SUCCESS;
}
