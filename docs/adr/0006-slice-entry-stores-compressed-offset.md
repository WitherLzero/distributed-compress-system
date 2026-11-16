# 0006 - 切片条目显式存储压缩后偏移

- 状态：已接受（2026-11-16）
- 背景：文件头切片条目原定 8B/条（compressed_len + crc32），压缩数据区内切片位置由前缀和推导。该推导使随机读取第 N 片需累加前 N−1 片长度，定位代价 O(N)，与存储系统随机读定位的基本要求不符。
- 决策：SliceEntry 扩为 16B/条 = compressed_offset(u64) + compressed_len(u32) + crc32(u32)，头部新增 compressed_len(u64)（压缩数据区总长，完整性校验）。切片定位 O(1)，一致性自检恒等式 offset[i]+len[i]==offset[i+1]、S0.offset==128+16×slice_count。
- 理由与先例：RocksDB BlockHandle{offset,size}、Parquet ColumnChunk.file_offset、ZIP central directory 均显式存储偏移；LZ4/Zstd frame 无偏移索引故只能顺序解压，反证随机读容器必带偏移表。crc32 保留在条目内维持 gzip 式端到端校验，且 16B 恰好 8 字节对齐。
- 代价：4KB 头+条目合计预算内容量由 496 条降为 248 条；4MB 切片下仍支撑块至约 1GB，预算余量充足。
- 明确不采纳：仅存 offset（8B/条、容量 496 条）——crc32 无处安放，破坏条目 8B 形状与对齐；origin_offset 仍不存——定长切片恒为 序号×slice_size，尾片长由 origin_block_size 推导，无场景需要显式存储。
- 影响：docs/design/design-document.md §7.1；meta_header 代码与单测同步（week4 issue 04）；week3 issue 02-D 布局结论与本 ADR 冲突处以本 ADR 为准（superseded）。
