// 压缩服务 RPC 调用面：对 CompressExecutor 暴露的压缩请求签名。
// 编码/连接/重试的执行层全在 IRpcAdapter 侧（唯一接缝，W5 接 BRPC、测试接 Fake），本类只定调用面。
#pragma once

#include "compress/error_code.h"

namespace compress::system {
class CompressRequest;  // proto 前向声明（生成头在构建期产出）
class CompressResponse;
}  // namespace compress::system

namespace compress {

class IRpcAdapter;  // RPC 接缝接口：W5 定稿声明，此处仅前向（设计文档 §7.2 注）

class CompressServiceRpcStub {
 public:
  explicit CompressServiceRpcStub(IRpcAdapter* adapter);

  // 压缩请求调用：同步语义，超时/失败经返回值上抛，由调用方决定重试或放弃
  CompressErr Compress(const ::compress::system::CompressRequest& req,
                       ::compress::system::CompressResponse* resp) noexcept;
};

}  // namespace compress
