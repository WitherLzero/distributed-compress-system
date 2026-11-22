// compress_server 侧（票 06 / CP4）：Compress RPC 服务实现——延迟 done 模式。
// handler 只做打包入队（微秒级，不占 brpc 线程做毫秒级压缩）；done 随堆上任务交给
// worker 线程在压缩完成后触发。队满走 ELIMIT 快路径：立即 SetFailed + Run。
#pragma once

#include "compress_service.pb.h"
#include "worker_pool.h"

namespace compress {

class CompressServiceImpl : public system::CompressService {
 public:
  explicit CompressServiceImpl(WorkerPool& pool) : pool_(pool) {}

  void Compress(google::protobuf::RpcController* cntl,
                const system::CompressRequest* req, system::CompressResponse* resp,
                google::protobuf::Closure* done) override;

 private:
  WorkerPool& pool_;
};

}  // namespace compress
