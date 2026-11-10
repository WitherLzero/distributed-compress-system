# 分布式透明压缩系统（Distributed Compression System）

面向类 HDFS 存储集群的透明压缩子系统：以 client 静态库形式嵌入 DataNode，后台扫描冷数据块、调度远端压缩节点完成 LZ4/LZMA 压缩，读路径透明解压。设计文档见 [docs/design/design-document.md](docs/design/design-document.md)。

项目当前处于骨架阶段：构建链、跨进程消息契约、编解码与文件格式等基础设施已就位并通过全部测试，数据路径正在按里程碑推进。

## 环境依赖

- Linux、CMake ≥ 3.16、支持 C++17 的编译器
- 第三方库：BRPC、protobuf、glog、gflags、LZ4、liblzma；测试需 gtest
- 完整搭建步骤见 [docs/environment-setup.md](docs/environment-setup.md)（含 BRPC 源码构建）

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
