#include "compress_service_impl.h"

#include <butil/iobuf.h>
#include <brpc/controller.h>

#include "worker_pool.h"

namespace compress {

void CompressServiceImpl::Compress(google::protobuf::RpcController* cntl_base,
                                   const system::CompressRequest* req,
                                   system::CompressResponse* resp,
                                   google::protobuf::Closure* done) {
  auto* cntl = static_cast<brpc::Controller*>(cntl_base);
  auto task = std::make_unique<RpcCompressTask>();
  task->done = done;        // 句柄三件套移交堆上对象——handler 返回后框架仍持有请求，
  task->cntl = cntl;        // done->Run 之前它们必须活着且只由 worker 触碰
  task->resp = resp;
  task->payload = cntl->request_attachment();  // IOBuf 移动：4MB 零拷贝
  task->level = req->level() == system::DEEP ? CompressLevel::kDeep : CompressLevel::kShallow;

  const auto enqueued = pool_.TryPush(std::move(task));
  if (enqueued != WorkerPool::EnqueueResult::kOk) {
    // 拒绝走 RPC 层错误码（§5.5），业务响应体不填——失败走 cntl 错误通道。
    // 满=kFull→ELIMIT；停机中=kClosed→ELOGOFF（client 可区分：满立即改投，
    // 退出中改投且对该实例短期降权）。
    if (enqueued == WorkerPool::EnqueueResult::kClosed) {
      cntl->SetFailed(brpc::ELOGOFF, "compress server shutting down");
    } else {
      cntl->SetFailed(brpc::ELIMIT, "compress worker queue full");
    }
    done->Run();  // 快路径立即触发：不是所有路径都延迟，但每条路径都恰好一次 Run
    return;
  }
  // 入队成功：什么都不触发，done 的所有权已随 task 交给 worker——
  // handler 在此返回，brpc 线程即刻解放。
}

}  // namespace compress
