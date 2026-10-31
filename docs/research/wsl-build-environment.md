# WSL2 编译环境搭建（issue 04 实操记录）

> 本文档是 `.scratch/week2-design-finalization/issues/04-build-environment.md` 的交付物之一：
> 环境搭建的完整步骤与实测结果，供 W3 "打通编译流程"复用。
> 关联调研：`docs/research/dependency-libraries.md`（B.4 依赖清单、B.5 易踩的坑）、
> `docs/research/external-systems.md` §5（WSL2 与 Docker、9P 性能陷阱、agent 操作 WSL 实测）。
> 配套脚本与冒烟测试工程：`local/env-setup/`（本地目录，不追踪）。

## 0. 决策记录（先定结论）

| 项 | 决策 | 理由 |
|---|---|---|
| 发行版 | **Ubuntu 22.04** | gcc 11.4（BRPC 官方测试范围 5.0–11.2 内）、cmake 3.22（BRPC 要求 3.16...3.28 内且有余量）、protobuf 3.12（< 4.21，不需要 absl/C++17 特殊处理）；与 BRPC 官方 Dockerfile 基础镜像一致 |
| BRPC 版本 | apache/brpc（master @ `be10f51`，2026-11-01） | baidu/brpc 已迁 Apache |
| leveldb / openssl | **不可禁用，装系统包**（`libleveldb-dev` / `libssl-dev`） | ⚠️ 预研判断被实测修正：master 已**无** `WITH_LEVELDB`/`WITH_OPENSSL` 选项且两者为硬性依赖（`src/brpc/CMakeLists.txt` leveldb 无条件链入、`src/butil/CMakeLists.txt:86` OpenSSL REQUIRED）。issues/05 #27 自决项在新版不可执行，改为安装但运行时不用（RPC 不走 TLS、不用 leveldb 存储） |
| 代码位置 | 编译放 WSL 侧 `~/`（ext4），不放 `/mnt/e` | 9P 跨文件系统编译慢 5~10 倍（external-systems.md §5.4） |
| 冒烟测试 | hello-world Echo service（proto3 + attachment 回环） | 同时验证链接、protobuf 生成、运行、**附件机制**（本项目技术核心，dependency-libraries.md A.4） |

## 1. 安装 WSL2（Windows 侧，一次性，需管理员 + 可能重启）

```powershell
# 管理员 PowerShell：
wsl --install -d Ubuntu-22.04
# 装完按提示重启；首次启动 Ubuntu 会要求设置 Linux 用户名/密码
```

> 2026-11-01 实测：本机 WSL 组件此前**未启用**（external-systems.md §5.1 记录的"组件已就位"已过时）。

## 2. 工具链与依赖（WSL 内，一条脚本）

```bash
# local/env-setup/setup-wsl-env.sh
sudo apt update && sudo apt install -y \
    build-essential cmake ninja-build git pkg-config \
    libgflags-dev libprotobuf-dev protobuf-compiler libprotoc-dev \
    liblz4-dev liblzma-dev libssl-dev libleveldb-dev
```

版本基线（2026-11-01 `env-report` 实测）：gcc/g++ 11.4.0 / cmake 3.22.1 / ninja 1.10.1 / protobuf+libprotoc 3.12.4 / gflags 2.2.2 / lz4 1.9.3 / xz 5.2.5 / ssl 3.0.2 / leveldb 1.23。

## 3. 编译安装 BRPC（WSL 内）

```bash
git clone --depth 1 https://github.com/apache/brpc.git ~/deps/brpc
cmake -B build -G Ninja -S ~/deps/brpc \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_DEBUG_SYMBOLS=OFF \
    -DBUILD_BRPC_TOOLS=OFF
cmake --build build -j"$(nproc)"
sudo cmake --install build    # 装到 /usr/local
```

要点：
- `WITH_DEBUG_SYMBOLS=OFF`：BRPC 默认开 `-g`，纯装环境不需要调试符号，省编译时间。
- 头文件装到 `/usr/local/include/brpc`，静态库 `/usr/local/lib/libbrpc.a`。
- **BRPC 不提供 CMake config 包**（只装 pkg-config 的 `brpc.pc`）→ `find_package(BRPC)` 需要自带 `FindBRPC.cmake`（dependency-libraries.md B.5 坑 #2 预判成立）。
- 静态链接 `libbrpc.a` 需要传递依赖 `leveldb/ssl/crypto/z`（master 把它们链进了库），`FindBRPC.cmake` 已处理，W3 直接复用。
- protobuf 3.12 会刷一批 `ByteSize()` deprecated 警告，无害，可忽略。

## 4. 冒烟测试（hello-world BRPC server+client + 附件回环）

工程在 `local/env-setup/hello-brpc/`：`echo.proto` + `server.cpp` + `client.cpp` + `CMakeLists.txt` + `cmake/FindBRPC.cmake`。

```bash
cp -r <repo>/local/env-setup/hello-brpc ~/hello-brpc && cd ~/hello-brpc
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON   # B.5 坑 #4
cmake --build build -j"$(nproc)"
./echo_server &        # 起服务（端口 8000）
./echo_client          # 同步调用 + 1MB 附件回环校验，期望输出 round-trip OK
```

验收口径：client 输出响应消息一致 **且** 附件字节级一致（attachment 是数据面大 payload 的传输机制，必须此刻就验证通）。

## 5. 实测记录

| 项 | 实测值 |
|---|---|
| 日期 | 2026-11-01 |
| BRPC commit | `be10f511c9fb8d490841cec027b54e706433766d` |
| gcc / g++ / cmake / ninja / protobuf / libprotoc | 11.4.0 / 11.4.0 / 3.22.1 / 1.10.1 / 3.12.4 / 3.12.4 |
| 其余依赖 | gflags 2.2.2、lz4 1.9.3、xz 5.2.5、ssl 3.0.2、leveldb 1.23 |
| BRPC 编译 | 388 个编译目标全绿（仅 protobuf 3.12 deprecation 警告），装到 `/usr/local` |
| 冒烟测试结果 | ✅ proto message round-trip OK；✅ attachment round-trip OK（1048576B 字节级一致）；client 输出 `ALL PASSED` |
| 坑与解法 | ① master 硬性依赖 OpenSSL → `libssl-dev`；② mcpack2pb 找 `libprotoc.so` → `libprotoc-dev`（Debian 拆包，开发符号链接单独发行）；③ master 无条件链 leveldb → `libleveldb-dev`；④ 冒烟工程 CMakeLists 源文件名笔误（`server.cpp` 写成 `echo_server.cpp`）；⑤ 静态链 `libbrpc.a` 需补 `leveldb/ssl/crypto/z` 传递依赖（已沉淀进 `FindBRPC.cmake`） |

完整版本清单存于 WSL 侧 `~/env-report.txt`（由 `setup-wsl-env.sh` 自动生成）。

## 6. W3 交接清单

> 双副本协作的持久约定（git 拓扑、两侧分工、纪律、坑速查）统一维护在
> `docs/agents/dev-workflow.md`——本节只列环境侧事项。

- [x] `find_package(BRPC)` + 自带 `FindBRPC.cmake` 已在冒烟测试验证（含静态传递依赖），W3 工程脚手架可直接复用
- [x] proto 生成 + `${CMAKE_CURRENT_BINARY_DIR}` include（B.5 坑 #3）已验证
- [x] LZ4/LZMA 系统包版本已确认可用（`liblz4-dev` / `liblzma-dev`）
- [x] agent 经 `wsl.exe -d Ubuntu-22.04 -- bash -lc "..."` 可免密执行编译（external-systems.md §5.5）；已配 `rynnli` NOPASSWD sudo
- [ ] 仓库已建 GitHub private remote，WSL 侧克隆 `~/projects/distributed-compress-system`
