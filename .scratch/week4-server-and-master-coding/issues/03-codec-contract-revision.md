# 03 — codec 接口返回值与序列化契约修订

- **Status**: completed（2026-11-26：五项落地于 commit 5bc1221；第 2 项参数类型维持 uint8_t* 待导师确认）
- **依据**: `meeting/见识星球 昕棋 软件研发实习第4课.md`（2026-11-14），时间戳 19:57–22:28、34:04–38:20、29:39–30:16
- **讨论记录**: 2026-11-16 与学员逐项过完六个子题，结论如下（含业界对照）

## What to change（定稿结论）

### 1. 返回值契约 ✅ 已定

现状：`Compress/Decompress` 返回 `size_t n`（成功=写入字节数，失败=0）+ `err` 出参 + `out_len` 出参，同一信息三通道重复（成功时返回值与 `*out_len` 相等；失败时返回值恒 0 无信息量），调用方无论如何必须先看 `err`。

改为（动机：三通道收敛为两通道，各通道语义单一）：

```cpp
virtual CompressErr Compress(const char* in, size_t in_len, char* out,
                             size_t out_cap, size_t* out_len) noexcept = 0;
```

- 返回值 = 错误码（kOk / kInvalidArg / kBufSmall / kIo），调用方先判返回值，成功后读 `*out_len`；
- **删除 `err` 出参**（返回槽改派错误码后自然消失，6 参降 5 参）；
- **保留 `out_len` 双语义**（这是当初造出这个参数的真实需求，勿删）：成功 = 实际写入字节数；`kBufSmall` = 所需容量（压缩侧为 `LZ4F_compressFrameBound`/`lzma_stream_buffer_bound`，LZ4 解压侧为帧头 `contentSize`）。

### 2. 参数类型 ⚠️ 待导师确认（默认建议 void*，不改也可）

导师原话：`uint8_t*`「业界少见，没见过这么用的」。调研结论（2026-11-16）：

- 指针算术三者等价（步长均 1），导师课上「in+1 移 1 字节」的说法只覆盖了这一层；
- 业界无统一：LZ4 老 API 用 `char*`（LZ4F 已弃用此风格）、liblzma/zlib 用 `uint8_t*`、zstd/LZ4F/POSIX 收敛到 `void*`；
- `char*` 缺陷：`char` 符号性实现定义（x86 signed / ARM unsigned），逐字节运算可能踩符号坑；
- **建议**：`void*`（站上"容器/传输层不解释内容"的正确抽象位，与两个后端的现代化 API 对齐）；学员若偏好低风险可维持 `uint8_t*`（lzma 同款，技术无损）。

**执行前提**：与导师对齐口径后再动；导师若坚持 `char*` 照办（无技术风险，注释注明按字节缓冲使用）。

### 3. out_cap 保留 ✅ 已定（不改接口，只补注释）

导师课上疑问「空间已在外部分配，out_cap 是否用不上」的答复：**谁分配与被调方要不要知道大小是两回事**——裸指针不带长度信息，`out_cap` 是被调方执行边界检查的必要输入。代码事实：`codec.cc:29` 前置检查 `out_cap < bound`；`codec.cc:35/115/137` 透传给 `LZ4F_compressFrame`/`lzma_easy_buffer_encode` 的 dstCapacity。业界无一例外：LZ4F/zstd/lzma/brotli/POSIX read 全带输出容量；省略容量的只有"返回值即新分配缓冲"风格（与我们 noexcept+零分配纪律冲突）。

解压侧正常路径缓冲必然够（定长切片 → 输出上界 `min(4MB, 块尾余量)` 精确已知），`out_cap` 仍兜两个底：坏帧产出长度异常时转 `kInvalidArg` 而非越界写；读路径任意窗口（宿主 cap < 4MB 时）。

动作：codec.h `out_cap` 注释补一句「边界检查依据：底层 dstCapacity 与 kBufSmall 精确回报均依赖此参数」。

### 4. 参数约定 ✅ 已定（通过）

输入 `const`（引用或指针），输出指针——一眼区分可动/不可动。不改接口形状之外的东西。

### 5. 序列化确定性契约 ✅ 已定

导师课上撞到的问题：同输入两次序列化字节不同。根因：`CompressedFileHeader` 的 `reserved[100]` 与对齐洞带 malloc 垃圾，`memcpy` 整结构体时一并进入输出。**不是**对 MB 级输出缓冲做 memset（压缩完整覆写输出，POSIX 契约即"只保证前 out_len 字节"）。

业界标准修法（两处动作）：

1. **构造点值初始化**：`CompressedFileHeader h{};`（128B 全零含 padding），或序列化函数内 `memset(&h, 0, sizeof h)` 后再填字段——只有 128B，代价可忽略；protobuf C++/内核同款惯例；
2. **codec.h 声明写入契约**（照 POSIX read 措辞）：「被调方负责写出恰好 `*out_len` 字节；`[out_len, out_cap)` 区间内容未指定，调用方不得读取」。调用方预清零**不要求**。

单测：同输入序列化两次，断言逐字节相同。

### 6. temp 文件目录 ✅ 已定（HDFS 式同卷专用目录）

历史澄清：现状同目录方案（`mkstemp("<target>.tmp.XXXXXX")`）不是疏忽，头注释明写约束——POSIX `rename(2)` 跨文件系统失败（EXDEV）。导师建议"单独 temp 目录"与该约束不冲突：**约束是同一个文件系统（挂载点/数据卷），不是同一个目录**。

采纳 HDFS datanode 模式（卷内 `tmp/` → `current/`，rename 同卷合法）：

```
<数据卷>/
├── blocks/    ← 最终压缩块文件
└── .tmp/      ← AtomicReplacer 临时文件（同卷，rename 可达）
```

实现变更（atomic_replacer.cpp + Options）：

1. temp 目录成为 `Options` 配置项，默认 `<目标目录>/.tmp/`（不存在则创建）；
2. `Open()` 时 `stat` temp 目录与目标文件，比对 `st_dev`（内核文件系统编号），不同卷直接返回 `kIo` fail-fast——防误配到 `/tmp`（tmpfs，另一文件系统）导致 rename 必炸；
3. 临时文件名沿用 mkstemp 模板 `<temp_dir>/<target_basename>.tmp.XXXXXX`；
4. 收益：目标目录列表永远干净（扫描器/其他工具不见半成品）、失败清理 = 清空一个目录。

单测：temp 目录隔离（目标目录无半成品残留）、同卷 `.tmp` 正常替换、模拟跨卷（`st_dev` 不同）拒绝。

## 约定

- 代码在 WSL 侧 `src/`、`include/compress/`，走 `feat/` 分支；agent 经 `wsl` 工具操作；
- 纯逻辑改动必须带 gtest 单测更新，`ctest` 全绿，`-Wall -Wextra` 零警告；
- 调用点适配（Server/Client 尚未写，现存调用点只有 codec_test 与未来路径）随本票完成。

## Acceptance

- [x] `Compress/Decompress` 返回值改为 `CompressErr`，`err` 出参删除，`out_len` 双语义注释明确，调用点全部适配
- [ ] 参数类型按导师确认结论改定（`void*` 为默认建议项），调用点适配
- [x] codec.h `out_cap` 注释补边界检查依据说明
- [x] 序列化处值初始化（`{}` 或 memset 结构体），codec.h 写入契约注释，双序列化逐字节一致单测
- [x] AtomicReplacer temp 目录配置化 + `st_dev` 同卷校验，单测三条（隔离/替换/跨卷拒绝）
- [x] `ctest` 一键全绿，`-Wall -Wextra` 无警告

## Comments
