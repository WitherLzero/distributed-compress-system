# 03 — 工程脚手架与公共/基础类

- **Status**: completed（2026-11-10：验收六条全绿；PR #1 经系统性双轴评审 + 学员终审后合并回 main）
- **Owner**: 林昕祺（代码在 WSL 侧，agent 经 `wsl.exe -d Ubuntu-22.04` 协助）
- **依据**: 实习大纲 W3；grilling 共识（2026-11-06 会话：验收线、公共类清单、单测门槛、分支策略、.so/空壳形态、构建系统、质量级别、基础设施形态）
- **阻塞**: 文件头结构体的落地依赖 issue 02-D 定稿；其余可并行

## Goal

WSL 侧仓库具备完整构建链，三个二进制（`compress_master` / `compress_server` / `mock_datanode`）+ `libcompress_client.so` 可一键编译、可启动、可干净退出（**骨架级，零业务逻辑预填**）。

## Deliverables

| 交付物 | 落点（WSL 侧仓库） | 验收 |
|---|---|---|
| 目录结构 | `src/{common,client,master,server}`, `proto/`, `mock/`, `cmake/` | 结构与设计文档 §7.2 类/文件映射对应 |
| 构建脚本 | 顶层 `CMakeLists.txt` + 模块 `add_subdirectory` + 复用 `FindBRPC.cmake`（自 `local/env-setup/hello-brpc` 搬入） | 干净 clone 一键 `cmake -B build && cmake --build build` |
| proto 编译接入 | `proto/*.proto` → 构建期生成 `.pb.cc`（protobuf 3.12 CMake 模块），不提交生成产物 | 冒烟：proto 序列化/反序列化可执行 |
| codec（LZ4/LZMA 统一编解码封装） | `src/common/codec.h/.cpp` | 编译通过 + round-trip 单测 |
| 文件头结构 + 读写 | `src/common/file_header.h/.cpp` | 编译通过 + 写入/读回一致性单测（布局以 issue 02-D 为准） |
| atomic_replacer | `src/common/atomic_replacer.cpp`（唯一文件，ADR-0003 纪律） | 编译通过（IO 类，单测留联调） |
| CompressTaskQueue | `src/common/task_queue.h`（header-only 有界队列） | 编译通过 + 有界行为单测 |
| 基础设施三件套 | glog + gflags + `InitInfra` 启动胶水（`src/common/init.h`）+ 错误码枚举（§5.5） | 三二进制启动时打出关键配置项日志 |
| 三个二进制 + client .so | `build/compress_master`, `build/compress_server`, `build/mock_datanode`, `build/libcompress_client.so` | 均可启动、SIGTERM/SIGINT 干净退出 |
| gtest 单测 | `tests/` + ctest | 纯逻辑类单测 `ctest` 一键通过 |
| 工程卫生 | `.clang-format`、`-Wall -Wextra` 无警告、README 构建说明 | mentor clone 后 10 分钟内可构建、能跑、能看懂 |

## Acceptance

- [x] WSL 侧干净 clone 后一键构建成功，README 写明环境依赖（指向 `docs/research/wsl-build-environment.md`）。
- [x] `compress_master` / `compress_server` / `mock_datanode` 三二进制 + `libcompress_client.so` 可编译；二进制启动、等信号、干净退出（启动日志含关键配置项）。
- [x] codec / 文件头 / CompressTaskQueue 三个纯逻辑类各有最小 gtest 单测并 `ctest` 通过（codec 覆盖 LZ4/LZMA round-trip）。
- [x] proto 冒烟测试通过（替代 `local/env-setup/hello-brpc` 的临时验证，该目录保持不入库）。
- [x] `.clang-format` 生效；`-Wall -Wextra` 零警告。
- [x] 代码走 `feat/w3-scaffold` 分支，周末合并回 main（Conventional Commits）。

## Out of scope

- server / master / client 业务逻辑（W4 / W5）——**零预填**，第 3 课修订会改 proto 与接口形态，先写业务等于重写。
- client .so 与 mock_datanode 的 dlopen 对接（待 issue 02-A C++ 接口定稿）。
- `IRpcAdapter` / `FakeRpc` 测试缝（W5 随 client 一起做）。
- 令牌桶流控实现（文档已定可选）。
- atomic_replacer 单测（IO 类，W5 联调验证）、GitHub Actions CI（W6 再评估）。

## Key constraints

- **不自产 logger 类**：glog（`LOG(INFO)` 全局宏，BRPC 同源）+ gflags（`DEFINE_*` 定义即配置）+ `InitInfra(argc, argv)` 胶水三件套；自产错误码 `enum class CompressErr` + 描述函数。
- 质量级别按 grilling 共识："mentor/面试官 clone 下来 10 分钟内能构建、能跑、能看懂"——RAII、无裸 `new/delete`、职责注释。
- 双副本约定：本 issue 的所有产物在 WSL 侧；Windows 侧只动 `.scratch/` 与 `docs/`。
- 压缩输出 buffer 按设计文档教训预留 2× 原始大小（issue 01 #7，实现期纪律）。

## 执行中可裂解

若执行中显大，按 tracker 惯例裂为 03a（脚手架 + 编译打通）/ 03b（公共类 + 单测），不算返工。

## 干净 clone 复验结果（2026-11-10，票 09）

复验对象：`feat/w3-scaffold` 分支 tip（2026-11-10 复验时点；不钉死 SHA，随提交演进），`git clone` 至全新目录 `/tmp/clean-verify`，从零执行：

- configure：`cmake -B build` exit=0
- 构建：`cmake --build build -j4` exit=0，警告数 0（`-Wall -Wextra` 全局生效）
- 测试：`ctest --test-dir build` 5/5 套件全过（codec 15 用例 / 文件头 6 用例 / 队列 6 用例 / replacer 7 用例 / proto 冒烟 3 用例，合计 37）
- 三二进制启动 + SIGTERM 干净退出：exit=0，日志 `stopped, signal=15`
- 产物齐全：`libcompress_client.so`、`compress_master`、`compress_server`、`mock_datanode`
