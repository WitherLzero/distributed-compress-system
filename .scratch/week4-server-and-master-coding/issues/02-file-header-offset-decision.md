# 02 — 文件头切片条目改存 offset：决策重议

- **Status**: completed（决策已定稿并全部落盘；代码侧实现移交 issue 04）
- **依据**: `meeting/见识星球 昕棋 软件研发实习第4课.md`（2026-11-14），时间戳 13:58–15:46

## 背景

第 3 课遗留决策 issue 02-D（W3 已定稿）将文件头布局定为：128B 定长头 + 每切片 8B 条目，**偏移量不存、由顺序与定长推导**；`issues/06-codec-and-file-header.md` 按此实现。

第 4 课导师否决了"存压缩长度、累加推导"的方案：

- 随机读 offset N 时需累加前 N-1 段压缩长度才能定位（64MB 逐段读到 64MB 处要加 63 次），CPU 浪费。
- 业界通行做法是**直接存压缩后数据的 offset**，数组直接定位。
- 副作用：条目从 8B 变宽（压缩长度 + offset 至少 16B/条），4KB 预算内可容纳的切片条目数减半，需重新核算布局。

## Goal

重新定稿文件头布局（"存 offset" 方案），并回填所有依赖决策的落点：

1. 设计文档 §7（关键数据结构）文件头字段表
2. W3 issue 02-D 结论标注 superseded
3. W3 issue 06 的表述与实现（WSL 侧 `meta_header` 代码 + 单测）
4. 视结论新增 ADR（记录"为何从推导改为存 offset"）或直接修订既有决策记录
5. GLOSSARY.md 术语表"元信息"条目的布局描述

## Acceptance

- [x] 新布局字段表定稿（含条目宽度、4KB 预算重核算）
- [x] issue 02-D 与 issue 06 回填
- [x] ADR 或决策记录落盘（ADR-0006）
- [x] GLOSSARY 同步

## Comments


### 2026-11-16 决策：方案 A（offset + size 双存）

定稿为业界通例形态（RocksDB BlockHandle 式）：

- `CompressedFileHeader` 128B 不变，新增 `compressed_len`（u64，压缩数据区总长，取自 reserved）；
- `SliceEntry` 16B/条 = `compressed_offset`(u64) + `compressed_len`(u32) + `crc32`(u32)：随机读 O(1)，crc 天然入条目且 8B 对齐；
- 4KB 合计预算重核算：容量 248 条（(4096−128)/16），64MB 块 @4MB 切片 = 384B，可支撑块至约 1GB；
- `origin_offset` 维持不存：定长切片恒为 序号 × slice_size，尾片长由 `origin_block_size` 推导，删除决定不受第 4 课翻案影响；
- 备选方案 B（仅存 offset 8B/条、容量 496 条）因 crc 无处安放（破坏 8B 条目形状）而弃选。

已落 `docs/design/design-document.md` §7.1；代码侧（`meta_header` 布局与单测，WSL 侧）变更记录于 issue 04，暂不动代码。
