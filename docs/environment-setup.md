# 环境搭建指引

在干净的 Linux 机器（物理机、虚拟机或容器均可）上从零搭建本项目的构建与测试环境。

验证基线：Ubuntu 22.04，gcc 11.4、cmake 3.22、protobuf 3.12、BRPC `be10f511`。

## 1. 安装系统依赖

```bash
sudo apt update && sudo apt install -y \
    build-essential cmake ninja-build git pkg-config clang-format \
    libgflags-dev libgoogle-glog-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev \
    liblz4-dev liblzma-dev libssl-dev libleveldb-dev \
    libgtest-dev
```

说明：

- `libprotoc-dev`：BRPC 的 mcpack2pb 需要 `libprotoc.so`（Debian 系将开发符号链接单独发行）。
- `libssl-dev` / `libleveldb-dev`：BRPC 无条件链接 OpenSSL 与 leveldb，构建期即需要。
- `libgoogle-glog-dev`：本项目直接使用 glog（BRPC 发行版未必开启 `BRPC_WITH_GLOG`）。
- `libgtest-dev`：仅运行测试需要；构建本体不依赖。

## 2. 编译安装 BRPC

BRPC 无预编译发行包，需从源码构建安装：

```bash
git clone https://github.com/apache/brpc.git ~/deps/brpc
cd ~/deps/brpc
git checkout be10f511c9fb8d490841cec027b54e706433766d   # 已验证的基线提交
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_DEBUG_SYMBOLS=OFF \
    -DBUILD_BRPC_TOOLS=OFF
cmake --build build -j"$(nproc)"
sudo cmake --install build     # 装到 /usr/local
```

说明：

- `WITH_DEBUG_SYMBOLS=OFF`：BRPC 默认带 `-g`，仅构建环境不需要调试符号。
- 安装产物：头文件 `/usr/local/include/brpc`，静态库 `/usr/local/lib/libbrpc.a`。
- BRPC 只安装 pkg-config 描述（`brpc.pc`），**不提供 CMake config 包**；本仓库自带 `cmake/FindBRPC.cmake` 完成 `find_package(BRPC)`，并已登记静态库的传递依赖（leveldb/ssl/crypto/z），无需额外处理。

## 3. 验证

```bash
git clone <本仓库> && cd <仓库目录>
cmake -B build && cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

预期：构建零警告退出，全部测试通过。
