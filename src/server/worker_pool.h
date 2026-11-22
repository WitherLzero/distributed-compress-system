// compress_server 侧（票 06 / CP4）：压缩工作线程池。
// RPC 线程生产、worker 消费；有界队列满即拒绝（ELIMIT 背压源，§5.5）。
// CPU 并发被线程数封顶；Stop 先关入口再排干存量——排干即优雅语义的落点。
#pragma once

#include <butil/iobuf.h>
#include <brpc/controller.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "compress/codec.h"

namespace google::protobuf {
class Closure;
}

namespace compress::system {
class CompressResponse;
}

namespace compress {

// 堆上任务对象：生命周期必须超过 Compress handler 的 return——
// done 触发之前，cntl/resp/attachment 归 brpc 框架所有（票 06 验收第 4 条的代码落点）。
// 本对象是它们在 worker 侧的唯一合法持有者；done 必须恰好一次 Run：
// 压缩成功 / 压缩失败 / 停机丢弃，三条路径一条都不能漏。
struct RpcCompressTask {
  google::protobuf::Closure* done = nullptr;
  brpc::Controller* cntl = nullptr;
  system::CompressResponse* resp = nullptr;
  butil::IOBuf payload;  // attachment 零拷贝转存（IOBuf 移动只换块链所有权）
  CompressLevel level = CompressLevel::kShallow;
};
using RpcCompressTaskPtr = std::unique_ptr<RpcCompressTask>;

class WorkerPool {
 public:
  WorkerPool(std::size_t worker_count, std::size_t queue_capacity);
  ~WorkerPool() { Stop(); }

  WorkerPool(const WorkerPool&) = delete;
  WorkerPool& operator=(const WorkerPool&) = delete;

  /// 非阻塞入队：满或已关闭立即拒绝（false）。成功后所有权移交池。
  bool TryPush(RpcCompressTaskPtr task);

  /// 幂等停机：关队列入口 → 唤醒 → 等 worker 排干存量退出 → 残余逐个补 Run(FAILED)。
  void Stop() noexcept;

 private:
  void WorkerLoop();
  static void FailTask(RpcCompressTaskPtr task, const char* msg) noexcept;

  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<RpcCompressTaskPtr> queue_;
  std::size_t capacity_;
  bool closed_ = false;  // 锁内裸 bool：所有读写都在 mu_ 临界区内（stop 三形态之第一形态）
  std::vector<std::thread> workers_;
};

}  // namespace compress
