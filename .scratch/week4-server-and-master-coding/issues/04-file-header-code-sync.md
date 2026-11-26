# 04 — 文件头布局改存 offset：代码侧同步

- **Status**: completed（2026-11-26：16B 条目代码同步与单测落地，commit 5bc1221）
- **依据**: issue 02 决策（2026-11-16，方案 A 定稿）；设计文档 §7.1 已更新

## 背景

第 4 课反馈否决"存压缩长度、前缀和推导 offset"的条目布局（随机读 O(N)），issue 02 已定稿方案 A：`SliceEntry` 16B/条 = compressed_offset(u64) + compressed_len(u32) + crc32(u32)，头部新增 `compressed_len` 字段（reserved 相应缩减至 100B）。设计文档已同步，**代码侧暂不动**，本票记录后续实现变更。

## 涉及代码（WSL 侧仓库）

1. `meta_header`（W3 issue 06 落地的文件头结构体与单测）：
   - `SliceEntry` 布局 8B → 16B，新增 `compressed_offset` 字段；
   - `CompressedFileHeader` 新增 `compressed_len`（u64），`reserved` 108B → 100B；
   - 单测更新：容量断言 496 → 248 条；64MB@4MB 布局体积断言 256B → 384B；
   - 新增一致性自检用例：offset[i] + len[i] == offset[i+1]，S0.offset == 128 + 16 × slice_count。
2. 写路径（atomic_replacer / temp 回填逻辑，若已触达条目回填）：append 时记录压后偏移，头部回填压缩总长。
3. 读路径（若已实现条目定位）：改为按 `compressed_offset` 直接 pread，删除前缀和推导。

## Acceptance

- [x] `meta_header` 结构体与磁盘布局对齐方案 A（16B 条目 + 头部 compressed_len）
- [x] 单测更新并通过（容量、体积、一致性自检）
- [x] W3 issue 06 表述回填 superseded 标注

## Comments
