// 块 → 版本号映射：压缩中途被读的取消机制（设计文档 §3.1.4）。
// version 管「现在」——读路径命中即 Bump，在途压缩任务逐切片比对版本发现失效立即止损；
// atime 管「未来」——下轮扫描不再选中。块删除场景经 Remove 顺带覆盖。
#pragma once

#include <cstdint>
#include <string>

namespace compress {

class BlockMetaView {
 public:
  BlockMetaView() = default;

  std::uint64_t Version(const std::string& block_path) const noexcept;  // 查询当前版本号
  void Bump(const std::string& block_path) noexcept;    // 读路径命中：版本 +1
  void Remove(const std::string& block_path) noexcept;  // 块删除：清除版本记录
};

}  // namespace compress
