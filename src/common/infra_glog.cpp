#include "compress/infra.h"

// glog 版启动：与 brpc 内嵌 butil 日志互斥（双 -v 注册冲突，见票 05 欠账）。
// 独立成 TU：静态库按 .o 抽取——不调 InitInfra 的二进制（master/server）不会拉进本
// 目标文件，链接行也就无需 glog。
#include <gflags/gflags.h>
#include <glog/logging.h>

namespace compress {

void InitInfra(int* argc, char*** argv) {
  InitFlags(argc, argv);
  google::InitGoogleLogging((*argv)[0]);
  google::InstallFailureSignalHandler();  // 崩溃栈追踪，联调期定位用
  FLAGS_logtostderr = true;               // 开发期日志直出 stderr
}

}  // namespace compress
