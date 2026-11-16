# Week 4 — Server 与 Master 模块编码（含第 4 课反馈落地）

- **Status**: in-progress
- **Owner**: 林昕祺
- **依据**: 实习大纲 W4——"按设计文档完成 Server 与 Master 模块编码"（`meeting/实习工作安排大纲（飞书存档）.md`）；第 4 课导师反馈（`meeting/见识星球 昕棋 软件研发实习第4课.md`，2026-11-14）
- **导师课**: 第 4 课复盘 W3 进展（文档 v0.3 + 脚手架已合并，PR #1）

## Goal

两条线并行：

1. **第 4 课反馈落地**：导师对设计文档（v0.3）与 W3 已交付代码的反馈逐条处理（issue 01），其中**文件头切片条目改存 offset** 推翻了 issue 02-D 的定稿决策，需单独立项重新定稿（issue 02）；代码类反馈（接口返回值/参数类型/memset 契约/temp 目录）走一张实现票（issue 03）。
2. **Server / Master 编码**：按大纲推进 server 端核心逻辑与 master 端注册表/判死/健康下发（导师定调：先把通信搞通，后面 debug 才好弄；先 server 还是 client 都可）。具体工票待反馈落地后与学员切分。

## Deliverables

| 交付物 | 落点 | 状态 |
|---|---|---|
| 第 4 课反馈清单逐条处理 | 设计文档 + GLOSSARY + 本目录 issue | ⬜ issue 01 |
| 文件头 offset 决策重新定稿 | issue 02-D 结论修订 + issue 06 + 视情况新 ADR | ⬜ issue 02（待讨论） |
| codec 接口与序列化代码修订 | WSL 侧 `src/common/`，`feat/` 分支 | ⬜ issue 03 |
| server / master 可执行二进制 | WSL 侧（大纲 W4 交付物） | ⬜ 待切票 |

## Acceptance

- 第 4 课反馈无残留：issue 01 逐条勾验（含扫描周期调整、X time 命名确认、流控表述确认）。
- offset 决策有明确结论并回填 issue 02-D / issue 06 相关表述（issue 02）。
- codec 单测全绿且覆盖新的返回值契约（issue 03）。
- W4 大纲交付物：server 与 master 二进制可编译、可启动（具体验收线切票时定）。

## Out of scope

- client 侧扫描/执行/回写业务逻辑填肉（W5）。
- 分布式流控（导师确认线上系统需专门团队，本期不实现；单机流控维持"文档写、实现可选"）。
- 监控打点（导师：不用搞，就是打点）。
- 三方联调（W5）。

## Key constraints

- 沿用 W3 约定：代码只在 WSL 侧开发（`docs/agents/dev-workflow.md`）；纯逻辑类 gtest；`-Wall -Wextra` 无警告。
- BRPC 异步纪律（第 4 课提醒）：controller 等句柄不得提前释放，否则 crash——通信票落地时写入代码注释与验收项。
- master 查询原样返回 registry、不 re-filter（W3 已修 gap）；server 无条件返回压缩结果，决策在 client 侧。

## Dependencies

- W3 产出：v0.3 文档、脚手架（PR #1）、公共类 + 单测、issue 02-D 文件头定稿（被 issue 02 推翻重议）。

## Issues

| # | 标题 | 状态 |
|---|---|---|
| 01 | 第 4 课反馈——可直接处理项 | needs-triage |
| 02 | 文件头切片条目改存 offset——决策重议 | needs-triage（待讨论） |
| 03 | codec 接口返回值与序列化契约修订 | needs-triage |
