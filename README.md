# 分布式透明压缩系统（Distributed Compression System）

面向类 HDFS 存储集群的透明压缩子系统：以 client 静态库形式嵌入 DataNode，后台扫描冷数据块、调度远端压缩节点完成 LZ4/LZMA 压缩，读路径透明解压。设计文档见 [docs/design/design-document.md](docs/design/design-document.md)。

项目当前处于骨架阶段：构建链、跨进程消息契约、编解码与文件格式等基础设施已就位并通过全部测试，数据路径正在按里程碑推进。

## 环境搭建

在干净的 Linux 机器（物理机、虚拟机或容器均可）上从零搭建：

```bash
# 1. 系统依赖（Ubuntu/Debian；BRPC 的 mcpack2pb 需 libprotoc 开发符号、
#    构建期即链 OpenSSL 与 leveldb；glog 由项目直接使用）
sudo apt update && sudo apt install -y \
    build-essential cmake ninja-build git pkg-config clang-format \
    libgflags-dev libgoogle-glog-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev \
    liblz4-dev liblzma-dev libssl-dev libleveldb-dev \
    libgtest-dev

# 2. BRPC 源码构建（无预编译发行包；基线提交已验证）
git clone https://github.com/apache/brpc.git ~/deps/brpc
cd ~/deps/brpc
git checkout be10f511c9fb8d490841cec027b54e706433766d
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_DEBUG_SYMBOLS=OFF \
    -DBUILD_BRPC_TOOLS=OFF
cmake --build build -j"$(nproc)"
sudo cmake --install build     # 装到 /usr/local（头文件 brpc/ + 静态库 libbrpc.a）
```

说明：

- BRPC 只安装 pkg-config 描述、**不提供 CMake config 包**；本仓库自带 `cmake/FindBRPC.cmake` 完成 `find_package(BRPC)`，并已登记静态库的传递依赖（leveldb/ssl/crypto/z）。
- 版本基线（已验证）：gcc 11.4、cmake 3.22、protobuf 3.12、BRPC `be10f511`；其余发行版按需调整。

依赖清单：Linux、CMake ≥ 3.16、支持 C++17 的编译器；第三方库 BRPC、protobuf、glog、gflags、LZ4、liblzma；测试另需 gtest。

## 一键构建与测试

```bash
cmake -B build            # configure + generate
cmake --build build -j4   # 构建
ctest --test-dir build --output-on-failure   # 运行全部测试
```

## 构建产物

| 产物 | 位置（build 树内） | 说明 |
|---|---|---|
| `libcompress_client.so` | `build/src/client/` | client 共享库（对外接口的最终交付形态） |
| `compress_master` | `build/src/master/` | master 节点服务（注册表/判死） |
| `compress_server` | `build/src/server/` | 压缩节点服务（WorkerPool 执行压缩任务） |
| `mock_datanode` | `build/mock/` | DataNode 嵌入形态演示进程 |
| `*_test` | `build/tests/` | gtest 单测可执行文件 |

## 目录结构

```
include/compress/   client 侧公开头（对外接口 + 类声明骨架）
proto/              跨进程消息契约（compress_service.proto，单一事实源）
src/client/         DataNode 嵌入侧：ClientCore 装配点、扫描器、执行器、RPC 桩
src/master/         master：ServerRegistry、DeathScanner
src/server/         压缩节点：CompressServiceImpl、WorkerPool、HeartbeatReporter
src/meta/           文件头读写、AtomicReplacer（唯一 rename 通道）
src/common/         三件套：错误码 / Options / InitInfra
mock/               mock_datanode 进程
tests/              gtest 单测（纯逻辑叶子）
cmake/              FindBRPC.cmake（BRPC 无官方 CMake config）
docs/               设计文档与架构决策记录（ADR）
```
