# 02 — 依赖库调研（BRPC / CMake / 压缩库）

- **Status**: **completed**（A/B/C 三部分均完成；API 调用代码按约定留到 W4/W5）
- **Type**: research
- **Blocked by**: —

## 进展

| 章节 | 状态 | 落点 |
|---|---|---|
| **A. BRPC + Protobuf** | ✅ **完成** | `docs/research/dependency-libraries.md` §A |
| **B. CMake（+ Blade 对比）** | ✅ **完成** | 同上 §B |
| **C. 压缩库（LZ4 / LZMA）** | ✅ **完成**（API 代码待 W4/W5） | 同上 §C |
| D. 概念补充 | ✅ 已覆盖 | `docs/research/external-systems.md` |

### C 部分的主要结论（已归档）

- 🔑 **LZ4 窗口固定 64 KB**（格式写死，不可调）→ **分片必须 ≥ 64 KB**（建议 256 KB~4 MB）
- 🔑 **LZMA 字典 = 回溯窗口，存的是【原始数据】不是压缩后数据**
- 🔑 两条口诀：**压缩内存 ≈ 字典 × 10~12**；**解压内存 ≈ 字典 + 1 MiB**
- 🔑 LZ4 没有字典表 —— 因为窗口写死，这就是它快、也是它压缩率低的原因
- ⚖️ **深压建议 `-6`**（94/9 MiB）而非 `-9`（674/65 MiB）：**压缩只做一次、解压做 N 次**，热路径上的解压内存更要紧
- ⚠️ **不可压缩数据会膨胀 0.4%** → 元信息**必须有算法标记字段**（0=未压/1=LZ4/2=LZMA）
- 💡 文本日志压 10+ 倍与通用基准 2-3 倍不矛盾 —— 是数据性质不同（长距离重复 + 高可预测性）
- 📌 调研深度约定：**权衡为主，不做算法实现**；Zstd 仅列入权衡表作参照，不作正式方案

### B 部分的主要结论（已归档）

- ✅ **确定用 CMake**：BRPC 官方自身即用 CMake，生态资料碾压 Blade，导师亦指定
- 🔑 **CMake 是"生成器"不是"构建工具"** → **不需要学 Makefile**（它是产物）
- 🔑 现代用法 = **target-based 三条命令**（`target_link_libraries` / `target_include_directories` / `target_compile_features`）
- 🔑 `PRIVATE` / `PUBLIC` / `INTERFACE` 判断法则：`#include` 在公开头文件 → PUBLIC；只在 `.cpp` → PRIVATE
- 📦 本项目 **5 个 target**：1 个动态库（`compress_client`）+ 4 个可执行
- ⚖️ **Blade**：大型 monorepo 专用（腾讯 3000 万行级）；强项"自动推导间接依赖"对我们 5 个 target 收益为负
- 🚨 **前置阻塞**：BRPC 不支持 Windows → 编译环境见 `external-systems.md` §5

### A 部分的主要发现（已归档）

- ⚠️ **默认消息上限 64 MB** —— 与我们 GB 级数据直接冲突，需分片或抬高上限
- 🚨 **默认 RPC 超时 500 ms**，而 LZMA 压 100 MB 需 ~100 秒 —— 不改超时必现重试风暴
- 🎁 **BRPC 自带反压**（`ELIMIT` + `max_concurrency`），无需自研
- 🎁 **优雅退出免费**（`ELOGOFF`）
- ⭐ **附件机制**：绕过 Protobuf 序列化，是大 payload 的正解
- ⭐ **我们对 master 与 BRPC 内置 LB 是两层**（架构层 vs 通信层）——解释了为何要自研 master
- ⭐⭐ **副作用在 client 不在 server** → server 天然幂等（结构性优势）
- 💀 **风险 1**：重试重新读盘 → 双重压缩（根因在 client）
- 💀 **风险 2**：lost update → 数据丢失（根因在 client，需版本号/CAS）
- ⚠️ **风险 3**：重复压缩白烧 CPU（RPC 层唯一遗留问题）

---

## 问题

本项目编译期依赖的第三方库各是什么、怎么用、有什么坑？产出 W1 要求的**技能调研文档**。

## 调研范围（原始清单）

### A. BRPC + Protobuf（**最高优先级**——全项目技术难点）

- [ ] BRPC 是什么、为什么各大厂用它（对比 gRPC / Thrift / srpc）
- [ ] **附件（attachment）机制怎么用**：`Controller::request_attachment()` / `response_attachment()`
  - 为什么压缩数据要走附件而非 Protobuf message？
  - 附件的容量上限与分片策略
- [ ] 定义 `.proto`、生成 C++ stub、同步/异步调用方式
- [ ] 服务端 / 客户端的最小可运行示例结构
- [ ] 内置能力：负载均衡策略、服务发现、超时与重试、内置服务页面
- [ ] 编译依赖链（protobuf / gflags / leveldb / ssl / …）与版本要求

### B. CMake（工程组织）

- [ ] 工程目录结构约定（W3 要用）
- [ ] 现代写法：`target_link_libraries` / `target_include_directories`（而非全局变量）
- [ ] **如何处理 BRPC 的重依赖**：vcpkg / Conan / FetchContent 三选一
- [ ] 多目标构建：master / server 两个可执行 + client 一个动态库 + mock 层
- [ ] 如何组织 `.proto` 的代码生成步骤

### C. 压缩库（LZ4 / LZMA）

- [ ] LZ4：库选型（官方 lz4 vs 其他封装）、API（`LZ4_compress_default` / `LZ4_decompress_safe`）、压缩级别
- [ ] LZMA：liblzma / xz-utils 的 C API、内存占用（**重要**：LZMA 压缩字典很大）、速度特性
- [ ] **算法原理**（面试必问）：LZ4 的哈希+匹配、LZMA 的区间编码与字典
- [ ] 常见算法速度/压缩比对照表（能报量级）
- [ ] 文本日志压缩率远高于通用语料的原因

### D. 概念补充（与外部系统衔接）

- [ ] HDFS 基本概念（NameNode / DataNode / block）——只需概念级
- [ ] MapReduce / Spark 解决什么问题——只需概念级
- [ ] 为什么集群有忙闲之分

## 产出

`docs/research/dependency-libraries.md`（或按主题拆成多份）

## Acceptance

- 能讲清 **BRPC 附件的用法**与适用场景。
- 给出 CMake 管理 BRPC 依赖的**具体方案**（不是"用 vcpkg 就行"这种程度）。
- 压缩算法有**对照表**，能回答"LZ4 和 LZMA 速度差多少、为什么用两个"。

## Comments

- 2026-10-19：创建。等背景讨论收尾后开始。
- 2026-10-19：**A 部分第一版完成** → `docs/research/dependency-libraries.md` §A（A.0–A.18）。
  方法：精读 BRPC 官方 server/client Basics 全篇，结合各轮讨论；期间修正一次错误归因（"双重压缩/lost update"曾被误记为 RPC 层问题，实际根因均在 client）。
  下一步：B（CMake，含 BRPC 依赖链管理具体方案）→ C（压缩库）。
- 2026-10-19：**B 部分（CMake + Blade 对比）完成** → `docs/research/dependency-libraries.md` §B（B.0–B.7）。
  要点：CMake 是生成器非构建工具（不必学 Makefile）；target-based 三条命令；PRIVATE/PUBLIC 判断法则；本项目 5 个 target；Blade 定位对比与不采用的 4 条理由。
  **附带发现（阻塞项）**：**BRPC 官方不支持 Windows**（vcpkg port 标注 `!windows`，依赖 epoll）→ 编译环境分析写入 `docs/research/external-systems.md` §5；同时补充了行业部署形态（§4.4）。
  下一步：C（压缩库 LZ4 / LZMA）。
- 2026-10-19：**C 部分（压缩库）完成** → `docs/research/dependency-libraries.md` §C（C.0–C.7）。
  深度约定与用户确认：**只需权衡 + 少数必要原理，不做算法实现**；**API 代码留到 W4/W5**；**Zstd 只进权衡表，不作正式方案**。
  关键产出：LZ4 64KB 窗口约束分片；LZMA 字典含义澄清（原始数据窗口）+ 内存口诀；深压选 `-6` 的理由；不可压缩数据膨胀 0.4% → 算法标记字段。
  **issue 02 至此完成。**
- 2026-10-30：第 2 课补充印证——导师确认分块压缩（64MB 按 1MB/4MB 一块，与 C 部分 LZ4 64KB 窗口约束一致）；原系统 LZMA→Zstd 部分迁移、业界主流 LZ4+Zstd；**面试需能清晰报出各算法压缩/解压速度与压缩率**（C 部分对照表正是为此准备，无需改动）。
