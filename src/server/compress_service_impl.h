// compress_server 侧骨架（票 08）：压缩 RPC 服务实现。
// 入队受并发上限约束，超限回 ELIMIT（设计文档 §7.2、§3.3.3）。
// BRPC/protobuf 类型仅前向声明：签名冻结即可，W5 接入 BRPC 时引入生成头。
#pragma once

namespace google::protobuf {
class RpcController;
class Closure;
}  // namespace google::protobuf

namespace compress::system {
class CompressRequest;
class CompressResponse;
}  // namespace compress::system

namespace compress {

class CompressServiceImpl {
 public:
  CompressServiceImpl() = default;

  void Compress(google::protobuf::RpcController* ctrl,
                const ::compress::system::CompressRequest* req,
                ::compress::system::CompressResponse* resp, google::protobuf::Closure* done);
};

}  // namespace compress
