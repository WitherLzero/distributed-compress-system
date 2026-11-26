# Week 4 — Server 与 Master 模块编码（含第 4 课反馈落地）

- **Status**: completed（2026-11-26：Server 与 Master 100% 收官，全票关闭；D5 push 待执行）
- **Owner**: 林昕祺
- **依据**: 实习大纲 W4——"按设计文档完成 Server 与 Master 模块编码"（`meeting/实习工作安排大纲（飞书存档）.md`）；第 4 课导师反馈（`meeting/见识星球 昕棋 软件研发实习第4课.md`，2026-11-14）
- **导师课**: 第 4 课复盘 W3 进展（文档 v0.3 + 脚手架已合并，PR #1）

## Goal

两条线并行：

1. **第 4 课反馈落地**：导师对设计文档（v0.3）与 W3 已交付代码的反馈逐条处理（issue 01），其中**文件头切片条目改存 offset** 推翻了 issue 02-D 的定稿决策，需单独立项重新定稿（issue 02）；代码类反馈（接口返回值/参数类型/memset 契约/temp 目录）走一张实现票（issue 03）。
2. **Server / Master 编码（本周里程碑）**：与学员对齐后的定稿口径——**本周 Server 与 Master 两个模块 100% 完成，不向 W5 留尾巴**；Client 模块一行不动（仅测试 harness 验证 Master 查询接口）。执行顺序：D1 契约收敛（issue 03/04）→ D2-3 Master 全部 → D3-4 Server 全部（真压缩端点，codec 现成不留占位）→ D5 多实例联调 + 全量单测 + push。

## Deliverables

| 交付物 | 落点 | 状态 |
|---|---|---|
| 第 4 课反馈清单逐条处理 | 设计文档 + GLOSSARY + 本目录 issue | ✅ issue 01 |
| 文件头 offset 决策重新定稿 | ADR-0006 + §7.1 + issue 04 代码同步 | ✅ issue 02 |
| codec 接口与序列化代码修订 | WSL 侧 `src/codec`、`src/meta` | ✅ issue 03（第 2 项类型选型待导师） |
| server / master 可执行二进制 | WSL 侧（大纲 W4 交付物） | ✅ issue 05 / 06 |
| Master-Server 多实例联调脚本 | WSL 侧 `tests/smoke_multi_instance.sh` + `tests/smoke_query.cc` | ✅ issue 07 |
| Client 测试 harness（无业务） | smoke_query CLI + server_compress 测试内真压缩往返 | ✅ 随 issue 07 |

## Acceptance

- 第 4 课反馈无残留：issue 01 逐条勾验（含扫描周期调整、X time 命名确认、流控表述确认）。
- offset 决策有明确结论并回填 issue 02-D / issue 06 相关表述（issue 02）。
- codec 单测全绿且覆盖新的返回值契约（issue 03）。
- W4 大纲交付物：server 与 master 二进制可编译、可启动，且两模块功能完成度 100%（验收线见 issue 05/06 各自 Acceptance）。
- **Master 完成定义**：注册→心跳刷新→判死扫描出列表→查询原样返回注册表全量（不排序不指定，选点在 client 侧随机），四态流转有单测覆盖；注册表不持久化，重启靠心跳自动重建。
- **Server 完成定义**：启动注册 + 周期心跳对真 master 验证；Compress RPC 真 LZ4 压缩端点（收 attachment → codec → 返回结果 + compressed_len），round-trip 单测背书；worker 线程池入队即拒绝（ELIMIT）。
- 多实例联调：脚本可拉起 N 个 server、杀死任意一个，master 列表在判死阈值内同步收缩（issue 07）。

## Out of scope

- client 侧扫描/执行/回写业务逻辑填肉（W5）。
- 分布式流控（导师确认线上系统需专门团队，本期不实现；单机流控维持"文档写、实现可选"）。
- 监控打点（导师：不用搞，就是打点）。
- 三方联调（W5）——本周的联调仅限 Master↔Server 双方 + 测试 harness，不涉及 Client 业务模块。
- Client 模块业务逻辑（扫描/队列/压缩流程/解压读路径）一行不动（W5）。
- 选点逻辑不落 Master：master 全量返回 WorkersInfo、client 随机自选（设计文档 §3.2.3/§6 proto 注释已定），本周无 client，故无可做项。

## Key constraints

- 沿用 W3 约定：代码只在 WSL 侧开发（`docs/agents/dev-workflow.md`）；纯逻辑类 gtest；`-Wall -Wextra` 无警告。
- BRPC 异步纪律（第 4 课提醒）：controller 等句柄不得提前释放，否则 crash——通信票落地时写入代码注释与验收项。
- master 查询原样返回 registry、不 re-filter（W3 已修 gap）；server 无条件返回压缩结果，决策在 client 侧。

## Dependencies

- W3 产出：v0.3 文档、脚手架（PR #1）、公共类 + 单测、issue 02-D 文件头定稿（被 issue 02 推翻重议）。

## Issues

| # | 标题 | 状态 |
|---|---|---|
| 01 | 第 4 课反馈——可直接处理项 | completed |
| 02 | 文件头切片条目改存 offset——决策重议 | completed |
| 03 | codec 接口返回值与序列化契约修订 | completed（第 2 项类型选型待导师） |
| 04 | 文件头 16B 条目——代码侧同步 | completed |
| 05 | Master 模块编码（注册表/判死扫描/查询下发） | ready-for-agent（依赖 03/04） |
| 06 | Server 模块编码（注册心跳/真压缩端点/线程池） | ready-for-agent（依赖 03/04） |
| 07 | Master-Server 多实例联调脚本 | ready-for-agent（依赖 05/06） |
