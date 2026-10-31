# 04 — 编译环境搭建（可延期至 W3）

- **Status**: completed（2026-11-01；验收两条均达成，实测记录见 `docs/research/wsl-build-environment.md` §5）
- **优先级**: 本周最末尾；时间不够顺延到 W3（大纲 W3 本就含"打通编译流程"，导师第 2 课亦提"剩余时间先编出二进制"）。

## 内容

1. WSL2 内搭 C++ 工具链（CMake + 编译器）。
2. 编译 BRPC（按 `docs/research/dependency-libraries.md` 的依赖清单；禁用 leveldb/openssl 可选项——#27 自决）。
3. 编译 LZ4 / LZMA（或确认系统包可用版本）。
4. 产出一个 hello-world 级 BRPC server+client 链接通过，验证环境可用。

## Acceptance

- WSL2 内 `cmake --build` 全链路通过，产出可执行文件。
- 环境搭建步骤记录成文档（`docs/research/` 或工程目录 README），供 W3 复用。

## Out of scope

- 工程框架初始化（W3 内容）。
