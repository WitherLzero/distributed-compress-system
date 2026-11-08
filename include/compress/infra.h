// 进程级基础设施装配胶水：gflags 解析 + glog 初始化 + 优雅退出信号等待。
// 仅用于带 main() 的二进制；库（compress_client）不依赖本头（§6.2.3 边界纪律 2）。
#pragma once

namespace compress {

// 解析命令行 flags、初始化日志；进程启动时调用一次。
void InitInfra(int* argc, char*** argv);

// 阻塞至收到 SIGINT / SIGTERM，返回收到的信号号（供退出码与日志使用）。
int WaitForShutdownSignal();

}  // namespace compress
