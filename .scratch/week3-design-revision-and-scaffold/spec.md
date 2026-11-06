# Week 3 — 设计文档修订（v0.3）与工程脚手架

- **Status**: in-progress
- **Owner**: 林昕祺
- **依据**: 实习大纲 W3——"搭建工程框架，完成公共类与基础类实现"（`meeting/实习工作安排大纲（飞书存档）.md`）；第 3 课导师反馈（`meeting/见识星球 昕棋 软件研发实习第3课.md`，2026-11-03）
- **导师课**: 第 4 课复盘本周进展

## Goal

两条线并行闭环：

1. **设计文档 v0.3**：吸收第 3 课全部反馈——14 条可直接处理项由 agent 执行、学员终审（issue 01），4 项遗留决策讨论定稿后落文档（issue 02），改完发群。
2. **工程脚手架 + 公共/基础类**（issue 03）：WSL 侧仓库具备完整构建链，三个二进制（`compress_master` / `compress_server` / `mock_datanode`）+ `libcompress_client.so` 可一键编译、可启动、可干净退出；公共类落地并带最小单测。

## Deliverables

| 交付物 | 落点 | 状态 |
|---|---|---|
| 设计文档修订 v0.3（14 条直接项） | `docs/design/design-document.md` + `docs/diagrams/` + `docs/adr/` + `GLOSSARY.md` | ⬜ issue 01 |
| 第 3 课遗留决策定稿（4 项） | 同上（随 v0.3 一起落） | ⬜ issue 02（待讨论） |
| 工程脚手架（目录 + CMake + proto 接入） | WSL 侧 `~/projects/distributed-compress-system` | ⬜ issue 03 |
| 公共/基础类 + gtest 单测 | WSL 侧 `src/common/` 等 | ⬜ issue 03 |
| 三个骨架二进制 + client .so | WSL 侧 `build/` | ⬜ issue 03 |

## Acceptance

- 设计文档无第 3 课反馈残留：14 条直接项逐条落文档（issue 01 Acceptance 逐条勾验），4 项遗留决策有明确结论（issue 02）。
- ADR-0003 状态改为 superseded，新增 ADR-0005（client 对外接口回 C++）。
- WSL 侧干净 clone 后 `cmake -B build && cmake --build build` 一键成功。
- `compress_master` / `compress_server` / `mock_datanode` 三个二进制 + `libcompress_client.so` 可编译；二进制可启动、SIGTERM/SIGINT 干净退出。
- codec / 文件头 / CompressTaskQueue 三个纯逻辑类各有最小 gtest 单测，`ctest` 一键跑过。
- 仓库根 README 含构建说明（环境依赖指向 `docs/research/wsl-build-environment.md`）。

## Out of scope

- server / master / client 业务逻辑（W4 / W5，导师已定"可以开始实现了"但按大纲节奏推进）。
- client 与 mock 宿主的 dlopen 对接（待 issue 02-A 的 C++ 接口形态定稿）。
- `IRpcAdapter` / `FakeRpc` 测试缝（W5 随 client 一起做）。
- 单机流控（令牌桶）实现——设计文档已定"文档要写、实现可选"。
- GitHub Actions CI（W6 求职资产阶段再评估）。
- issue tracker 迁移 GitHub Issues——**本期不迁**（remote 已存在，但本地 md 在 git 内可随双副本同步，且主要消费者是 agent + 学员；W6 收尾时再评估）。

## Key constraints

- **双副本约定**：代码只在 WSL 侧开发，agent 经 `wsl.exe -d Ubuntu-22.04` 操作；文档/issue/ADR 在 Windows 侧（`docs/agents/dev-workflow.md`）。
- **分支策略**（本轮新定）：代码走 `feat/w3-scaffold` 分支，周末合并回 main；docs/issue 小改动直接 main。
- **质量级别**（本轮新定，grilling 共识）：按"mentor/面试官 clone 下来 10 分钟内能构建、能跑、能看懂"的标尺——`.clang-format` 统一、`-Wall -Wextra` 无警告、RAII、Conventional Commits。
- **基础设施形态**：不自产 logger 类；glog（日志）+ gflags（配置）+ `InitInfra` 启动胶水 + 自产错误码枚举（§5.5）。
- **测试**：gtest + ctest；只给纯逻辑类（codec / 文件头 / 任务队列）写单测，IO/RPC 类留联调。
- proto 编译产物构建期生成，不提交生成代码。
- issue / 目录命名英文小写 kebab-case。

## Dependencies

- W2 产出：设计文档 v0.2、三张架构图、ADR-0001~0004、WSL2 + BRPC 编译环境（`@be10f51`，hello-brpc 冒烟通过）。
- issue 03 中**文件头结构体**的落地依赖 issue 02-D（4K 布局定稿）；其余脚手架工作可与 01/02 并行。

## Issues

| # | 标题 | 状态 |
|---|---|---|
| 01 | 第 3 课文档修订——可直接处理项（14 条） | ready-for-agent |
| 02 | 第 3 课遗留决策——待讨论定稿（4 项） | in-progress（待讨论） |
| 03 | 工程脚手架与公共/基础类 | in-progress |
