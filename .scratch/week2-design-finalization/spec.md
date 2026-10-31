# Week 2 — 设计文档定稿

- **Status**: in-progress
- **Owner**: 林昕祺
- **依据**: 实习大纲 W2——"设计文档编写完成（定稿）"（`meeting/实习工作安排大纲（飞书存档）.md`）
- **导师课**: 第 3 课从头过设计文档（草稿发群 → 导师离线审 → 课上过）

## Goal

把 `docs/design/design-document.md` 从 v0.1 草稿推进到**可提交定稿**：销号全部【待确认】标记、补完第 7/8 章、吸收第 2 课全部新增决策项，并完成架构图重绘。

## Deliverables

| 交付物 | 落点 | 状态 |
|---|---|---|
| 设计文档定稿（8 章完整、无待确认标记） | `docs/design/design-document.md` | ✅ v0.2 完成（issue01 关账；正文无待确认标记，7/8 章落稿） |
| 第 2 课新增决策项落文档 | 流控、深压解压位置、mock 形态、分块大小、监控说明 | ✅ 5 条定稿（见 issues/02；流控=§3.1.5、深压=本地为主、监控不加小节） |
| 架构图重绘（含新增流程图） | `docs/diagrams/`（架构图 + 压缩主流程图 + 失效判定图，正文 §1.3/4.2/4.4 引用） | ✅ 完成（issue03；正文图 1/2/3 已插图） |
| 编译环境（WSL2 + BRPC） | 本地环境 | ✅ 完成（issue04，2026-11-01；Ubuntu 22.04 + BRPC master @be10f51，hello-brpc 冒烟通过） |

## 参考文档速查（原设计文档附录 B，定稿时移入本处）

| 文档 | 用途 |
|---|---|
| `docs/diagrams/compress-system.drawio` | 架构图（导师提供） |
| `docs/diagrams/flow-compress.drawio` | 压缩主流程图（4.2） |
| `docs/diagrams/flow-death.drawio` | 实例失效判定流程图（4.4） |
| `docs/research/external-systems.md` | 外部系统与部署形态调研 |
| `docs/research/dependency-libraries.md` | RPC 框架、构建工具、压缩算法调研 |
| `docs/adr/0001-local-mock-external-clusters.md` | 本地 mock 方案决策 |
| `docs/adr/0003-c-abi-client-interface.md` | client 对外 C ABI 接口决策（选型过程见 `docs/design/client-api-alternatives.md`） |
| `docs/adr/0004-proto3-syntax.md` | 接口描述 proto3 语法选型决策 |
| `.scratch/week1-background-and-design/issues/05-mentor-clarifications.md` | 待确认事项完整清单（原附录 A 的汇总表以此为准） |

## Acceptance

- 设计文档无任何【待确认 #N】未销号；附录 A 全部改为已答复/已决策。
- 第 7 章含：压缩元信息文件头（struct 级）、三方 proto、关键类/函数声明（不写实现）、线程模型、队列结构；proto header 含 version + trace id。
- 第 8 章按"无历史版本"简化写法（导师已认可）。
- 草稿发群，等导师离线审 + 第 3 课反馈；反馈后的修订不在本周期强制闭环。

## Out of scope

- 不写工程代码（W3 起）。
- 流控**只写文档不实现**。
- 监控采集不实现（生产由专门系统承担；文档仅说明指标定义与外置决策）。

## Key constraints

- 设计文档精度要求（导师第 2 课）：struct 定义、三方 proto、关键类/函数声明（不写实现）、线程模型（master leader tracker、server heartbeat 线程）、队列结构。
- issue / 目录命名英文小写 kebab-case；commit 遵循 Conventional Commits。
- `meeting/` 与 `local/` 不追踪。

## Dependencies

- W1 产出：设计文档 v0.1、调研文档、ADR-0001/0002、已闭环的问题清单（17 完整答复 + 9 条自决/删除）。
- 第 3 课时间点（导师反馈节奏）。

## Issues

| # | 标题 | 状态 |
|---|---|---|
| 01 | 设计文档内容确认与最终决策 | ✅ completed（commit 3a3f10c） |
| 02 | 第 2 课新增决策项（5 条） | ✅ completed |
| 03 | 架构图重绘 | ✅ completed（commit 26b5acf；三图定稿并入 docs/diagrams/） |
| 04 | 编译环境搭建（可延期至 W3） | ✅ completed（2026-11-01；冒烟测试 proto + 1MB attachment 双验证通过） |
