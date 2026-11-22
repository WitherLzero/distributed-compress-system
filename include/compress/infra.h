// 进程级基础设施装配胶水：gflags 解析 + glog 初始化 + 优雅退出信号等待。
// 仅用于带 main() 的二进制；库（compress_client）不依赖本头（§6.2.3 边界纪律 2）。
#pragma once

namespace compress {

// 解析命令行 flags、初始化日志；进程启动时调用一次。
// 注意：会初始化 glog——与 brpc 内嵌 butil 日志互斥（双 -v 注册冲突），
// master/server 等链接 BRPC 的二进制请改用 InitFlags。
void InitInfra(int* argc, char*** argv);

// 仅解析命令行 flags，不初始化任何日志库——glog-free 二进制（master/server）用这个。
void InitFlags(int* argc, char*** argv);

// 安装 SIGINT/SIGTERM 处置（只装 handler 立即返回，标志位由 WaitForShutdownSignal 消费）。
// 必须在任何网络服务启动前调用：信号处置在安装前是内核默认值（TERM=直接杀），
// 启动窗口内落下的信号若未安装只会暴毙，优雅退出语义失效。
void InstallShutdownHandlers();

// 阻塞至收到 SIGINT / SIGTERM，返回收到的信号号（供退出码与日志使用）。
int WaitForShutdownSignal();

}  // namespace compress
