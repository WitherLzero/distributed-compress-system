# 设计决策依据对照表

> 用途：为 `docs/design/design-document.md` 的每个设计决策标注业界先例/出处，供自审与第 3 课答辩引用。不随设计文档交付，属内部依据。
> 分类：**A 架构层**（系统级，导师已确认为主）、**B proto 层**、**C++ 类/线程层**。每条标注：先例做法 → 本项目做法 → 差异与理由。

## A. 架构层

| 决策 | 业界先例 | 本项目 | 差异与理由 |
|---|---|---|---|
| 心跳保活 + 超时判死 | HDFS DataNode 心跳（默认 3s，超时踢出）、etcd lease（TTL 到期即失效） | 心跳 5s / 判死 20s（×4） | 同一机制，阈值更激进；理由：压缩任务丢失代价低（幂等可重试），无需 HDFS 级别的保守 |
| 中心化轻量 master | HDFS NameNode（单主 + 内存元数据）、Redis Sentinel | 单实例 master，内存注册表 | 规模小（百级实例）撑得起单点；HA 缺失由"心跳自动重建"消化 |
| 选点 = random + 拒绝重试 | 无负载感知的随机放置是多前端系统默认；Nginx/Envoy 文档均承认 random 在均质后端下与最少连接接近 | master 给总表，client random | 无差异；负重视图滞后使精密算法失去意义 |
| 数据面/控制面分离 | 一切存储系统通则（NameNode 不搬 block） | 数据不经 master | 无差异 |
| 附件传大块数据 | brpc attachment 机制（官方推荐大块数据走 attachment 免序列化） | 压缩数据走 attachment | 无差异 |
| temp + rename 原子落盘 | POSIX 惯用法（SQLite journal、 RocksDB WAL 切换、各类编辑器保存） | 同 | 无差异；补充 datanode 启动清 temp 是该惯用法标准配套 |
| 块级压缩 + 元信息索引 | RocksDB SSTable（block + index）、Parquet（row group + footer） | 4MB 块 + 4KB 头部索引 | 它们索引放**尾部**（文件可追加），我们放**头部**（文件不可变、要边读边定位）；块大小量级一致（RocksDB 默认 block 4~16KB 属另一层级，其 SST 整体压缩单元与 4MB 同量级） |
| 不可压缩数据回退存储 | zstd/lz4 应用层惯例（比对压缩前后大小取小者） | NOT_BETTER 状态回退 | 无差异 |
| 闲时算力复用 | 各公司离线混部实践（在线集群闲时跑离线作业） | 复用离线集群 | 无差异 |

## B. proto 层

| 决策 | 标准做法出处 | 本项目 | 差异与理由 |
|---|---|---|---|
| 消息命名 | Protobuf 官方 Style Guide：消息 CamelCase、字段 snake_case、枚举值 SCREAMING_SNAKE | 同 | 无差异 |
| 通用头消息 | 各公司 proto 规范惯例（公共 Header message 携带 version/trace_id） | MsgHeader{version, trace_id} | 无差异；version 支撑演进、trace_id 支撑全链路日志 |
| 大数据不进消息体 | brpc 官方：大 blob 走 attachment；protobuf 官方：单消息不建议超几十 MB | 4MB 块走 attachment | 无差异 |
| ⚠️ **proto2 语法 vs proto3** | Google 现行默认 proto3；官方 Style Guide 明言 **required 字段已被弃用（never use required）**——字段语义变更时 required 是兼容性灾难 | 当前文档用 proto2 语法（required/optional） | **这是本项目与"现代标准做法"最大的偏离点**。理由：brpc 官方示例大量使用 proto2 + required/optional（brpc 对 proto2 兼容成熟）。两个选项：①跟 brpc 示例保持 proto2（学习材料多、与框架一致）；②改 proto3（singular 字段默认 optional 语义 + optional 关键字显式表达）。**建议②**，更靠近现代标准，brpc 完全支持 proto3。待定夺 |
| 枚举有零值 | protobuf 规范：枚举第一个值必须为 0，作默认值 | CompressLevel{SHALLOW=0}, CompressResult{OK=0} | 无差异 |
| 服务定义拆分粒度 | 按"调用方向 + 目标方"拆 service | MasterService / RegistryService / CompressService 三个 | 无差异；每条 RPC 通道一个 service，master/server 职责边界清晰 |

## C. C++ 类 / 线程层

| 决策 | 标准做法出处 | 本项目 | 差异与理由 |
|---|---|---|---|
| 动态库 C 边界 | 各大库通则（如 SQLite、leveldb 的 C 绑定）：C ABI 稳定、错误经返回码 | C 风格函数集 + 返回码 | 无差异；C++ 内部实现、C 外壳是标准分层 |
| 库不抛异常出边界 | C++ Core Guidelines（E.16 等）：模块边界 catch-all | 全接口捕获异常转返回码 | 无差异 |
| 有界队列满即拒 | 线程池标准语义（Java ThreadPoolExecutor AbortPolicy；各类 C++ 线程池实现） | client 队列丢弃候选、server 队列回 REJECTED | 无差异；两处"满"策略不同是业务差异（冷数据幂等 vs 服务容量信号） |
| 固定大小工作线程池 | 常规线程池实践；CPU 密集任务线程数 ≈ 核数 | WorkerPool 固定 N 线程 + 有界队列 | 无差异；深压缩按内存预算倒推 N 是本项目特有约束（LZMA preset 内存大） |
| bthread 不跑长 CPU 任务 | brpc 官方文档（bthread 是 M:N 协作调度，阻塞/长计算影响同线程其他 bthread） | 压缩落 std::thread 池，bthread 只做 RPC 收发 | 无差异；这是 brpc 用户的惯例架构 |
| 独立心跳线程 | 保活与业务解耦的通用做法 | HeartbeatReporter 独立线程 | 无差异；防"业务忙到心跳饿死被误判死" |
| 静态注册/启动自动重连 | 客户端类库惯例（启动即注册，不依赖持久状态） | 首跳即注册 | 无差异 |
| 符号可见性控制 | 库作者惯例（-fvisibility=hidden + 显式导出宏） | 导出符号受控 | 无差异；W3 构建时落地 |

## 使用方式

- 自审：每个"无差异"条目 = 与业界对齐，不需再议；每个"差异"条目 = 已写明理由，导师课上主动讲；
- 唯一开放项：**B 层 proto2/proto3 选型**——影响 §6.1 全部代码块与 W3 起步，建议第 3 课前定。
