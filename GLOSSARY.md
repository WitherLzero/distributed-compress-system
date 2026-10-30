# GLOSSARY.md

> 本项目的领域上下文。**读代码或改代码之前先读这份文件。**
> 详细论证见 `docs/research/external-systems.md`；设计决策见 `docs/adr/`。

## 项目是什么

**分布式压缩系统**（6 周 C++ 实习项目）。核心目标：**节省存储成本**——把长期无人访问的冷数据压缩掉。

**一句话定位**：

> 在**既有的**分布式存储集群 与 **既有的**离线计算集群之间，插入一套**闲时压缩服务**。

## 系统边界（最重要的一节）

| | 内容 |
|---|---|
| **要实现的** | `compress_client`（动态库）、`compress_master`、`compress_server` |
| **不实现的** | HDFS / 存储集群、MapReduce / Spark、调度器（YARN / K8s） |

三者都是**既有系统**：我们从存储集群**读写数据**，在离线集群**借住算力**。本地没有这两个环境，故用 mock 替代（见 `docs/adr/0001`）。

## 三个组件

| 组件 | 部署形态 | 生命周期 | 职责 |
|---|---|---|---|
| **compress_client** | **动态库**，链入 datanode 进程 | 随 datanode 存活 | 扫描冷数据、发压缩请求、收回结果并回写、访问时解压 |
| **compress_master** | 独立进程，**固定机器** | 常驻，**不能随意重启** | server 注册表维护、心跳判死、负载视图/排序 |
| **compress_server** | 独立进程，跑在离线集群 | **随时被拉起/杀掉**，无状态 | 执行压缩、心跳上报自身负载 |

## 外部系统（都是"环境"，不是"工具"）

| 系统 | 性质 | 对接形态 |
|---|---|---|
| **存储集群（类 HDFS）** | 分布式文件系统 | **API 对接**：读写数据 |
| **MapReduce / Spark** | 计算框架 | **无 API 对接**——只是"集群忙时的来源"，我们是它的邻居 |
| **调度器（YARN / K8s）** | 资源调度器 | **管理接口**：决定 server 何时生、何时死 |

> 关键：**MR/Spark 不提供任何我们要调的接口**。它们的意义是解释"这个集群为什么有忙闲之分"。

## 控制面 / 数据面分离

理解全系统的总钥匙：

```
             控制面（元数据，KB 级）
             ①心跳上报        ┌─ compress_master ─┐
     compress_server ─────────┤  · server 注册表   │
                              │  · 心跳超时判死     │
             ②get_workers_info │  · 负载视图        │
     compress_client ─────────└────────────────────┘
             │
             │ ③send data / ④response data（大 payload，GB 级）
             └──────────────────► compress_server
                    数据面（直连，不经 master）
```

- **master 永远不碰真实数据** —— 所以它能是"一台固定机器"
- **大流量走 client↔server 直连** —— 带宽压力不落在 master 上

## 术语表

| 术语 | 含义 |
|---|---|
| **datanode** | 存储集群中实际持有数据的节点；`compress_client` 作为 lib 嵌入其中 |
| **xtime** | 数据的"最后访问时间"，用于判定压缩触发条件（**第2课已澄清**：即文件系统自带 atime，block 元数据含 block id + xtime，client 只读不记） |
| **浅压缩** | 7 天无访问触发，LZ4，快（~1 GB/s），压缩率较低 |
| **深压缩** | 14 天无访问触发，LZMA，极慢（~1 MB/s），压缩率高 |
| **注册** | server 启动时向 master 声明"我是谁、我在哪"（一次性） |
| **心跳** | server 周期上报"我还活着 + 当前负载"（持续） |
| **判死** | master 依"最后一次心跳距今多久"推断 server 已死亡 |
| **反压（backpressure）** | server 队列过深时拒绝/延迟，促使 client 退避并换 server 重试 |
| **退避（backoff）** | client 遇忙后的等待再重试策略 |
| **附件（attachment）** | BRPC 传输大 payload 的机制；压缩数据走附件，控制消息走 Protobuf |
| **元信息** | 压缩后必须记录的信息（原始长度、压缩后长度、压缩单位等），否则无法解压 |

## 关键设计约束

- **压缩对用户完全无感**：解压必须在数据节点侧完成，不能让用户端承担解压 CPU。
- **server 无状态**：因为随时会被调度器杀掉，不能把中间状态只放在内存里。
- **master 是单点**：这是"用注册中心换 server 可任意启停"的必然代价（本期不做 HA）。

## 索引

| 想了解 | 去哪 |
|---|---|
| 外部系统详解、部署形态、完整时序 | `docs/research/external-systems.md` |
| 本地 mock 决策 | `docs/adr/0001-local-mock-external-clusters.md` |
| W1 任务与验收 | `.scratch/week1-background-and-design/spec.md` |
| 待向导师澄清的问题 | `.scratch/week1-background-and-design/issues/05-mentor-clarifications.md` |
| 工程约定（issue tracker / domain docs） | `docs/agents/` |
