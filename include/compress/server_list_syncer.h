// 实例列表同步线程：周期向 master 拉取 compress_server 实例列表存内存，
// 供压缩线程 random 选点；master 不可达时沿用本地缓存（设计文档 §7.2、§3.2.4）。
#pragma once

namespace compress {

class ServerListSyncer {
 public:
  ServerListSyncer() = default;

  void Run();  // 周期 GetWorkersInfo → 更新内存缓存列表
};

}  // namespace compress
