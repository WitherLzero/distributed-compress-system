# Week 1 — 背景对齐与设计文档启动

- **Status**: in-progress
- **Owner**: 林昕祺
- **依据**: 实习大纲 W1（`.scratch` 外：`meeting/实习工作安排大纲（飞书存档）.md`）
- **导师课**: 第 2 课复盘本阶段产出

## Goal

在导师第 2 课之前，完成 **① 项目背景与外部系统认知**、**② 依赖库技术选型调研**，并把**项目设计文档**推进到核心章节 ≥50%。

## Deliverables

| 交付物 | 落点 | 状态 |
|---|---|---|
| 外部系统认知（详细版） | `docs/research/external-systems.md` | ✅ 已完成 |
| 项目领域上下文（精炼版） | `CONTEXT.md` | ✅ 已完成 |
| 依赖库调研（BRPC / CMake / LZ4 / LZMA） | `docs/research/dependency-libraries.md` | 🔄 **A. BRPC 第一版完成**；B/C 待补 |
| 设计文档初稿（≥50%） | `docs/design/` 或仓库根（套用 `meeting/设计文档模版.md`） | ⬜ 未开始 |
| 架构图重绘 | `docs/compress-system.drawio`（规范化布局） | ⬜ 未开始 |
| 向导师澄清的问题清单 | `issues/05-mentor-clarifications.md` | 🔄 持续追加（已 24 条） |

## Acceptance

- 能向导师**口头讲清**：三个组件职责、系统边界、控制面/数据面分离、一次压缩的完整时序，并指出理解偏差。
- 调研文档能回答：**BRPC 的附件（attachment）怎么用**、CMake 如何管理 BRPC 的重依赖。
- 设计文档覆盖：需求背景、功能模块、接口定义、关键数据结构——足够支撑 W3 开始搭工程。
- 待澄清问题**已在第 2 课提出**并得到答复（或明确记录为"待定"）。

## Out of scope

- 不写代码（W3 才初始化工程）。
- 不实现 mock 层（W3 做，见 `docs/adr/0001`）。
- 不搭建真实 HDFS / Spark 环境。

## Key constraints

- **系统边界不得滑移**：HDFS、MR/Spark、调度器都是既有系统，本项目只做 compress 三组件（见 `CONTEXT.md`）。
- `meeting/` 与 `local/` 为本地目录，**不追踪**（见 `.gitignore`）。
- issue / 目录命名一律**英文小写 kebab-case**（见 `.scratch/README.md`）。
- commit 遵循 Conventional Commits，类型用英文。

## Dependencies

- 已具备：导师架构图、第 1 课会议记录、设计文档模板、实习大纲。
- 待获取：导师答复（见 `issues/05`）。

## Open decisions（尚未固化，等后续讨论）

- master vs client 的**决策位置**（选 server 的 score 放哪）。
- **server 无状态**的具体取舍（被 kill 时正在压的任务如何处理）。
- 深压缩是**叠加**在浅压结果上，还是先解压再深压。
- 压缩方案是否引入 **Zstd** 作为备选/替代。

## Issues

| # | 标题 | 状态 |
|---|---|---|
| 01 | 外部系统认知调研 | ✅ 完成 |
| 02 | 依赖库调研（BRPC / CMake / 压缩库） | ⬜ 待开始 |
| 03 | 设计文档初稿 | ⬜ 待开始 |
| 04 | 架构图重绘 | ⬜ 待开始 |
| 05 | 待向导师澄清的问题 | 🔄 持续追加 |
