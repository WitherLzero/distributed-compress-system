// compress_server 侧骨架（票 08）：压缩工作线程池。
// RPC 线程生产、工作线程消费，队列实体内嵌于本类（设计文档 §7.2、§7.4）。
// 深压缩内存预算按预设核算，入队即拒绝（ELIMIT，见 §3.3.4）。
#pragma once

namespace compress::system {
class CompressRequest;  // proto 前向声明
}

namespace compress {

class WorkerPool {
 public:
  WorkerPool() = default;

  // 提交压缩任务（设计文档 §7.2 的 Submit(Task&&)，任务负载 = 压缩请求）
  void Submit(::compress::system::CompressRequest&& request);
};

}  // namespace compress
