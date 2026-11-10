# 09: 工程卫生收口 + 干净 clone 复验

**Parent**: 03 — 工程脚手架与公共/基础类

**What to build**: W3 脚手架的最后收口：格式规范落地、全库警告清零、README 让导师/面试官 clone 后 10 分钟内完成"构建 → 跑起来 → 看懂结构"，并在 WSL 侧做一次干净 clone 的从零复验，最后按周节奏把功能分支合回主干。收口后 issue 03 的验收线全绿。

**Blocked by**: 05、06、07、08。

**Status**: ready-for-agent

- [x] 格式化规范文件落地并全库执行一遍
- [x] `-Wall -Wextra` 全库零警告（含三张前置工票的代码）
- [x] README：环境依赖（指向环境搭建文档）、一键构建命令、三二进制与动态库的产物说明、目录结构导读
- [x] WSL 侧干净 clone 从零构建 + 全部测试通过，结果贴回 issue 03
- [ ] 功能分支合并回主干，提交信息遵循 Conventional Commits
- [ ] issue 03 Acceptance 六条全绿并关闭，父 issue 状态由学员终审后更新

## 设计文档 §7.2 回写清单（评审沉淀，2026-11-10）

代码已先行落地、设计文档滞后的条目，合并前一并回写：

- ClientCore 四方法补 `noexcept`（ADR-0005 四纪律对齐；评审 A3）
- 任务队列签名 `Push/Pop` → `TryPush/Pop`（代码已按非阻塞语义命名，CP5 落地）
- `HealthyWorkers()` 命名评估（评审 C1：名字承诺过滤、注释承诺全量；改名如 `AllWorkers`/`Snapshot` 需学员拍板，不改则至少把语义写进方法名注释）

## PR #1 评审台账（系统性双轴评审，2026-11-10）

### A 组：合并前已修复（本分支）

- A1 mock_datanode.cpp 漏格式化 → 已 clang-format（CP7 格式化时漏 mock/ 目录）
- A2 FindBRPC.cmake 四个传递依赖未入 REQUIRED_VARS → 已补，缺库 configure 期即报错
- A3 磁盘契约两处偏离 → 学员拍板**改文档对齐代码**：§7.1 algorithm `0=LZ4, 1=LZMA`（CompressLevel 枚举原值，写盘零翻译表）；crc32 = 切片原始字节的 CRC32（解压后比对，gzip 式端到端校验——压后 CRC 拦不住解压器 bug 的错误输出）
- A4 测试计数 35 → 37（codec 参数化展开后 15 用例），issue 03 复验记录已修

### B 组：记账（不阻塞合并）

- 已顺手修：缺尾换行 ×3、`src/codec/CMakeLists.txt` 行尾空白、`Pop/Close/Size` 与 `Stub::Compress` 补 noexcept（`TryPush` 刻意不标：deque 分配可抛，诚实标记优先于形式统一）
- RunProcess() 抽取建议：**评审过、不采纳**——三个 main 的 starting 日志各自携带本进程配置项（port/master_addr），抽取会拆散「启动 + 配置」单行日志；5 行重复成本 < 信息损失
- 缓 W4/W5：六参组 Data Clump（ICodec 与头序列化的重复签名，动公共契约需设计决策）；票 04 配置项日志（Options 未被消费前无真实项可打，随 W4 接线落地）；infra.h 位于公开头树（组织性，W4 评估迁移）
- 流程项：交付物落点从 `src/common/` 漂移至 `src/codec|meta|include/compress/`（组织性偏差，README 已如实描述）；SHA 钉点教训（复验记录改记「分支 tip + 日期」，已改）；collaboration-protocol「文档直接 main」轻违规（合并后自然归位）；Conventional Commits 核验留待合并提交时

## W5 欠账（CP4 评审沉淀，2026-11-09）

- 输入截断的错误映射测试（LZ4 帧解析错 vs LZMA 按 in_pos/in_len 区分截断与容量不足）：格式相关实现细节，属读路径场景，W5 集成测试补。
- LZMA 解压侧 kBufSmall 的精确所需容量：xz 格式原始大小在流尾索引，one-shot API 不暴露；真实读路径以文件头 origin_block_size 为准，W5 接入后按设计文档复核 tests/codec_test.cc 中 Lz4DecompressSmallBufferReportsSize 的语义。
