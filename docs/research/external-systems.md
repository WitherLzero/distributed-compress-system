# 外部系统认知：存储集群、离线集群与调度器

> W1 调研文档（部分）。**精炼版见仓库根 `GLOSSARY.md`**；本文件是带论证的详细版。
> 唯一权威依据：导师手绘架构图 `docs/compress-system.drawio`（2026-10-18 第1课后由导师提供）。

## 0. 一句话定位

> 本系统是插在**既有的**分布式存储集群 与 **既有的**离线计算集群之间的**闲时压缩服务**。

**要实现的**：`compress_client`（动态库）、`compress_master`、`compress_server`。
**不实现的**：HDFS / 存储集群、MapReduce / Spark、调度器——这三者都是既有系统。

---

## 1. 四个名词的定性（先分清层级）

这几个词常被混用，但它们**不是同一层级的东西**：

| 名词 | 性质 | 开源 | 组成 |
|---|---|---|---|
| **HDFS** | **分布式文件系统** | ✅ Apache Hadoop | NameNode + DataNode + …（多个 Java 守护进程） |
| **MapReduce** | **计算框架**（批处理） | ✅ Hadoop MR | ApplicationMaster + Task 容器 |
| **Spark** | **计算框架**（批处理/流式） | ✅ Apache Spark | Driver + Executor |
| **YARN** | **资源调度器** | ✅ Hadoop YARN | ResourceManager + NodeManager |

> 术语校准：严格说"集群"指**机器**，"框架"指跑在上面的**软件**。导师说的"MR/Spark 集群"实指"装了 MR/Spark 的机器集群"。

**关键结论**：它们是**开源的、可部署的分布式系统**，每个由多个服务（守护进程）构成。

---

## 2. 三种对接形态（核心：它们**完全不同**）

这是本文档最重要的一节——**不要把它们当成同一种"依赖"**。

### ① 存储集群：**API 对接**（读写数据）

唯一真正需要调用其能力的系统。`compress_client` 需要：读待压缩数据、把压缩后数据写回（替换原数据）、写元信息。

### ② MapReduce / Spark：**无 API 对接**，只是"寄宿"关系 ⭐

**最反直觉、也最重要的一点**：

> **`compress_server` 根本不调用 MapReduce 或 Spark 的任何 API。**

它只是**跑在这些机器上**，作为一个普通 C++ 守护进程。为什么跑到那儿？因为那些机器在**闲时**有空闲 CPU / 内存。

MR / Spark 在这个架构里的角色是：**"让集群变忙的原因"**，是我们的**邻居（co-tenant）**，不是我们调用的工具。

```
       同一套物理集群（离线集群）
┌──────────────────────────────────────────┐
│   YARN 调度器（唯一的管理者）              │
│      ├── 拉起 MR 作业（白天业务高峰）      │  ← 忙时的来源
│      ├── 拉起 Spark 作业                  │
│      └── 闲时：拉起 compress_server       │  ← 我们（蹭空闲资源）
│   物理机群：CPU / 内存 / 磁盘              │
└──────────────────────────────────────────┘
```

**为什么课上会提到 MR/Spark**：为了说明"这个集群有忙闲之分"——忙是因为在跑 MR/Spark 作业，闲是我们才能用。**它们是背景，不是接口。**

**结论**：**不需要学 MapReduce 编程，也不需要学 Spark API。**

### ③ 调度器：**管理接口**（启停控制）

决定 server 何时生、何时死。真实生产可能是：
- **YARN**：REST API 提交/停止 application（[ResourceManager REST API](https://hadoop.apache.org/docs/stable/hadoop-yarn/hadoop-yarn-site/ResourceManagerRest.html)）
- **K8s**：Deployment / StatefulSet 伸缩
- **自研调度**：自有协议

本项目由 **mock_scheduler** 承担（见 `docs/adr/0001-local-mock-external-clusters.md`）。

---

## 3. 决定性技术事实：C++ lib **无法**嵌入真 HDFS

这条推论定死了 mock 的必要性：

> **HDFS 的 DataNode 是 Java 写的。** 我们的 `compress_client` 是 C++ 动态库，**没法把 C++ `.so` 链进一个 Java 进程**（除非用 JNI 做桥接，那是另一套复杂工程）。

| 通路 | 是什么 | 能否"嵌进 datanode" |
|---|---|---|
| [libhdfs](https://hadoop.apache.org/docs/stable/hadoop-project-dist/hadoop-hdfs/LibHdfs.html) | JNI 封装的 C API，供 C/C++ 程序**作为客户端**访问 HDFS | ❌ 是"外部客户端"，不是嵌入 |
| （无公开路径） | datanode 内部原生 API | ❌ Java 进程内部 |

**推断**：导师那套系统里 `compress_client` 能以 lib 形式嵌进 datanode，说明**那个 datanode 本身是 C++ 写的**（自研存储系统），drawio 上的"（HDFS）"是**类比说明**，不是字面意义的 Apache Hadoop。

> ⚠️ 此为推断，需向导师确认（见 `issues/05-mentor-clarifications.md`）。无论结论如何，**本地演示都必须自造 mock datanode**。

---

## 4. 部署形态

### 4.1 生产环境拓扑

以导师的图为唯一依据：

```
┌─────────────────────────────────────────────────────────────────┐
│  存储集群（HDFS）                           [机房 A / 网段 A]     │
│  ┌────────────────────┐   ┌────────────────────┐                 │
│  │ datanode #1        │   │ datanode #2        │  ...  N 个       │
│  │  └ compress_client │   │  └ compress_client │                 │
│  └─────────┬──────────┘   └─────────┬──────────┘                 │
│            │ get_workers_info / send_data                        │
└────────────┼────────────────────────┼────────────────────────────┘
             ▼                        ▼
      ┌───────────────────────────────────────┐
      │        compress_master                │   ← 固定机器
      │  · server 信息维护(IP/CPU/MEM/队列)    │     常驻、不随便重启
      │  · 心跳超时判死                        │     单点（本期无 HA）
      └───────────────▲───────────────────────┘
                      │ 心跳上报 / 物理信息采集 / 队列长度采集
┌─────────────────────┴───────────────────────────────────────────┐
│  离线系统（MR / Spark）                     [机房 B / 网段 B]     │
│  ┌─ compress_server ─┐ ┌─ compress_server ─┐  ... 动态启停        │
│  └────────────────────┘ └────────────────────┘                    │
│  ┌──────────────────────────────────────┐                        │
│  │  调度系统（YARN / K8s / 自研）         │  ← 只依赖，不实现       │
│  └──────────────────────────────────────┘                        │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 三个进程的部署特征对比

| | compress_master | compress_client | compress_server |
|---|---|---|---|
| **部署形态** | 独立进程 | **动态库（lib）** | 独立进程 |
| **实例数** | 1（本期无 HA） | 每 datanode 一个 | N 个，动态变化 |
| **所在位置** | 固定机器 | **嵌在 datanode 进程内** | 离线集群节点 |
| **生命周期** | 常驻，**不能随便重启** | 随 datanode 进程存活 | **随时被拉起/杀掉** |
| **有无状态** | 有（server 注册表） | 有（扫描进度/元信息） | 应尽量无状态 |
| **端口** | 固定监听 | 无需监听（主动连接） | 动态端口（注册给 master） |
| **启动顺序** | **最先** | 之后 | 最后（且反复启停） |

### 4.3 本地开发形态（与生产完全不同）

```
本机（单机多进程）
┌─────────────────────────────────────────────────┐
│  mock_datanode  ── 加载 compress_client.so       │
│  compress_client.so  ── 被上面加载               │
├─────────────────────────────────────────────────┤
│  compress_master  ── 独立进程，监听固定端口        │
├─────────────────────────────────────────────────┤
│  mock_scheduler ── 模拟"拉起/杀掉 server"         │
│         ├──► compress_server #1                  │
│         ├──► compress_server #2                  │
│         └──► compress_server #3                  │
├─────────────────────────────────────────────────┤
│  mock_storage ── 替代 HDFS（本地目录）            │
└─────────────────────────────────────────────────┘
```

### 4.4 行业部署形态：**分层，且三层各不相同**

> 证据基础：公开工程博客（Uber / Salesforce / LINE）+ Cloudera 参考架构 + 2026 年生态综述。属**行业观察**，非某公司内部数据。

**先分清两件事**（最容易混淆）：

| | 用途 | 行业普及度 |
|---|---|---|
| **Docker 作为开发环境** | 让所有人的编译环境一致 | ⭐⭐⭐ 几乎标准做法 |
| **容器作为生产部署形态** | 服务跑在容器里 | ⭐⭐ 看在哪一层 |

#### 存储层（HDFS DataNode）——大多仍是**裸金属**

| 原因 | 说明 |
|---|---|
| **本地磁盘亲和性** | DataNode 的价值就是"数据在本地盘"，容器化后存储要挂载、I/O 路径变长 |
| **大容量盘管理** | 一台机器挂 12~24 块大容量盘，硬件形态不适合容器抽象 |
| **性能敏感** | Cloudera 参考架构明确：DataNode 数据目录**应使用 local storage**，"保持计算贴近存储、不从远端读" |

> Uber 在[容器化 Hadoop NameNode](https://uber.com/en-AU/blog/hadoop-namenode-container) 时明确写道：NameNode 是"性能最敏感的组件"，他们把这一步**排在容器化旅程最后**。**连 NameNode 都要慎重，DataNode 更甚。**

#### 计算层（Spark / MapReduce）——**正在大规模容器化**

| 信号 | 现状 |
|---|---|
| **Spark on K8s** | K8s 已有**官方 Spark Operator**；云厂商重心基本在 K8s |
| **YARN** | 仍撑着大量 **on-prem Hadoop 集群**，"不会在这个十年内消失" |
| **迁移动因** | 资源争抢、弹性伸缩差、Hadoop 依赖重（如 [LINE 案例](https://techlist.io/en/posts/e01c2b9badf019ddd50b77a6e12d4ef8523fdb98)） |

**现状概括**：
- **云上 / 新建系统** → 容器化（K8s）为主
- **传统 on-prem 大数据集群** → YARN + 裸金属仍占大头

> ⚠️ 该集群是 **YARN 还是 K8s**，直接决定 `compress_server` 的真实部署形态 —— 待向导师确认（见 `issues/05` #26）。

#### ⭐ 我们的三组件恰好横跨这三层

| 组件 | 跟随谁 | 行业典型形态 | 原因 |
|---|---|---|---|
| **compress_client** | 跟随 **datanode** | **裸金属** | 宿主是存储节点（本身不容器化）；且它是 lib，不能独立部署 |
| **compress_server** | 跟随**调度器** | **容器**（若 K8s）/ **裸金属+YARN**（若 YARN） | 无状态、可随意 kill、动态启停 |
| **compress_master** | 固定机器 | 容器或裸金属（都可） | 有状态但状态轻量（注册表），不需本地盘 |

**🔑 一个漂亮的吻合**：`compress_server` 的特性几乎就是容器的定义——

| compress_server 特性 | 容器的对应特性 |
|---|---|
| 无状态（重启即全新） | 容器的默认假设 |
| 随时被拉起 / kill | 弹性伸缩 |
| 数量动态变化 | 副本数 |
| 不需本地持久化 | 无状态工作负载 |
| 启动后自报家门（注册） | 服务发现 |

> 若真实调度器是 K8s，`compress_server` 天然就是一个 `Deployment` 的 Pod。**这不是巧合——K8s 的调度模型与"闲时借算力"的模型本质相同。**
>
> 📌 值得写进设计文档：「**server 无状态的设计，使其天然适配容器化调度**」。

---

## 5. 本地开发环境（BRPC 的 Windows 阻塞）

### 5.1 🚨 硬事实：BRPC 官方不支持 Windows

**三条独立证据**：

**① vcpkg 的 brpc port 明确标注**（[vcpkg.json](https://github.com/microsoft/vcpkg/blob/master/ports/brpc/vcpkg.json)）：
```json
"supports": "!android & !windows"
```

**② BRPC 官方支持列表**（[getting_started](https://brpc.apache.org/docs/en/getting_started.html)）无 Windows：
```
Ubuntu/LinuxMint/WSL · Fedora/CentOS · Linux with self-built deps · MacOS · Docker
```

**③ 原理层面**：BRPC 的 IO 核心是 `EventDispatcher`，依赖 **epoll**（Linux 专属）的边缘触发模式（见 [IO 文档](https://brpc.apache.org/docs/rpc-in-depth/io/)）。Windows 无 epoll（它是 IOCP 模型，BRPC 未采用）。

**本机实际状况**：

| 项 | 状态 | 对 BRPC |
|---|---|---|
| 系统 | Windows 11 家庭中文版（build 26100） | ❌ |
| 编译器 | MSYS2 UCRT64 / **g++ 16.2.0** | ❌ 官方仅支持 GCC 5.0–11.2（建议 8.2+） |
| CMake 4.4.2 / Ninja | ✅ | ✅ |
| LZ4 / LZMA 头文件 | MSYS2 里**已有** ✅ | ✅ |
| protobuf / gflags / leveldb | ❌ 未装（MSYS2 仓库均有） | ✅ |
| WSL | ❌ 未装发行版（组件已就位） | —— |
| Docker | ❌ 未装 | —— |
| Visual Studio 2022 | 有 | ——（BRPC 不支持 MSVC） |

> 讽刺的是：**除 BRPC 之外的所有依赖，本机都能搞定**。卡点只有 BRPC 本身。

### 5.2 WSL2 与 Docker 的关系（关键认知）

**在 Windows 上，两者不是竞争关系，而是包含关系**：

```
        Windows 11
            │
     ┌──────┴──────┐
     │    WSL2      │   ← 一个真实的 Linux 内核（微软的轻量虚拟机）
     └──────┬──────┘
            │
     ┌──────┴─────────────────┐
     │  Docker Desktop 复用     │   ← 底层就是这个 Linux 内核
     │  它来跑容器              │
     └────────────────────────┘
```

> **Docker Desktop 在 Windows 上安装时会自带 WSL2 后端。** 所以"Docker 能解决 Windows 阻塞"的真实原因是：**它底层就是 WSL2**，只是在 Linux 之上又加了一层容器隔离与打包。

### 5.3 两者对比

| 能力 | **WSL2 直接装** | **Docker** |
|---|---|---|
| 环境快照/复制 | ❌ 手工 `apt install` 一堆 | ✅ `docker build` 一条命令 |
| 环境一致性 | ⚠️ 因人而异 | ✅ 同一 image 处处一样 |
| 多套环境并存 | ⚠️ 只能一套 | ✅ 20.04 / 22.04 / Alpine 同时存在 |
| 不污染宿主 | ⚠️ 包都留在 Ubuntu 里 | ✅ 容器删掉就干净 |
| CI/CD 复用 | ⚠️ CI 上要重配 | ✅ 同一 image 直接跑 |
| **开发体验** | ⭐⭐⭐ VS Code 直连，编辑/编译/调试如本地 | ⭐⭐ 文件要挂载，稍绕 |
| **调试能力** | ⭐⭐⭐ gdb / 断点原生可用 | ⚠️ 容器内调试需额外配置 |
| **性能** | ⭐⭐⭐ 文件在 Linux 文件系统内 | ⚠️ 跨文件系统挂载明显下降 |

**BRPC 官方两条路线都支持**：支持列表里有 "WSL"，仓库里也自带 `Dockerfile`（`FROM ubuntu:20.04`，已把构建环境配好）。

### 5.4 ⚠️ 文件系统的性能陷阱

WSL2 与 Windows **双向可见，无需配置**：

```
方向 ①：Windows 访问 Linux 文件
        \\wsl.localhost\Ubuntu\home\you\proj

方向 ②：Linux 访问 Windows 文件
        /mnt/e/Projects/...    （E 盘自动挂载）
```

**但性能差异巨大**：

| 场景 | 性能 | 原因 |
|---|---|---|
| Linux 进程读 **Linux 侧**文件（`~/proj`） | ⭐⭐⭐ 快 | 原生 ext4 |
| Linux 进程读 **Windows 侧**文件（`/mnt/e/...`） | 🐌 **慢 5~10 倍** | 经 9P 协议转换 |
| Windows 进程读 **Linux 侧**文件 | 🐌 慢 | 同样转换 |

> 对编译型项目影响显著：编译要读成千上万个头文件，每个都走 9P 转换。
>
> **本项目仓库当前在 `E:\Projects\...`（Windows 盘）** → 若在 WSL 编译，需决定代码放哪：
> | 方案 | 做法 | 编译速度 | 代价 |
> |---|---|---|---|
> | A. 留在 E 盘 | `cd /mnt/e/Projects/...` | 🐌 慢 | 零改动 |
> | B. 移到 WSL | `~/projects/...` | ⭐ 快 | Windows 侧需用 UNC 访问 |
> | C. 双位置 | E 盘为主 + WSL 内 clone | 快 | 需同步 |

### 5.5 Agent 能否操作 WSL（实测结论）

**能。** `wsl.exe` 是普通 Windows 可执行文件，`ops_bash`（MSYS2 bash）可直接调用：

```bash
wsl.exe -d Ubuntu -- bash -lc "cd ~/proj && cmake --build build -j16"
```

| 问题 | 答案 |
|---|---|
| 需要 SSH / 端口转发吗 | ❌ **不需要**，本地进程调用 |
| 需要密码登录吗 | ❌ 默认免密 |
| 目录共享吗 | ✅ 双向（`\\wsl.localhost\` ↔ `/mnt/`） |
| agent 能读写 WSL 文件吗 | ✅ 用 `wsl.exe` 进 Linux 侧读写，**无性能损失** |

**⚠️ 已知坑：编码**。`wsl.exe` 自身的管理类输出（`--status`、`--list`）是 **UTF-16LE**，MSYS2 bash 按 UTF-8 解析会乱码。解法（已验证）：
```bash
wsl.exe --status 2>&1 | iconv -f UTF-16LE -t UTF-8
```
> Linux 侧的**命令输出（编译日志等）是 UTF-8**，不受此影响。

**局限**：单次命令可以（编译、运行、查看输出）；**交互式会话**（如开 `gdb` 交互调试）用 `-c` 方式做不了，需人工开终端。

### 5.6 待定

环境方案尚未确定（WSL2 / Docker / 远程 Linux 主机 / 只出设计不出二进制），已记入 `issues/05` #25、#26。

> 💡 另有一条后续可选项：使用者手头有一台**同局域网 Linux 服务器**和另一台可跑 master 的笔记本 —— 未来可做**跨机联调**（client 在 Windows / server 在 Linux / master 在另一台）。因各机性能不同，需额外评估，暂不纳入 W1。

---

## 6. 控制面 / 数据面分离（理解全系统的总钥匙）

图上的箭头（依据 drawio XML 中 source/target 的精确方向）：

| 箭头 | 源 → 目标 | 标签 |
|---|---|---|
| ① | `compress_server` → `compress_master` | `1. 上报负载、IP` |
| ② | `compress_client` → `compress_master` | `2. get_workers_info` |
| ③ | `compress_client` → `compress_server` | `3. send data` |
| ④ | `compress_server` → `compress_client` | `4. response data` |

| | 走谁 | 传什么 | 量级 |
|---|---|---|---|
| **控制面** | client→master、server→master | 元信息（IP、负载、列表） | KB 级 |
| **数据面** | client→server | 压缩数据本体 | GB 级 |

> 🔑 **master 永远不碰真实数据**——这就是它能是"一台固定机器"的原因。也解释了 BRPC 附件的意义：大 payload 走附件，不塞进 Protobuf message。

---

## 7. 服务发现：为什么 master 地址必须固定

```
        ┌──────────── 固定地址 ────────────┐
   client ──── "有哪些 server 可用？" ────► master
   server ──── "我起来了，我在 X" ──────────┘（心跳持续）
```

| | 设计 A：client 直连配置的 server 列表 | 设计 B（本项目）：**master 做注册中心** |
|---|---|---|
| server 启停 | 要改所有 client 配置 | ✅ 自动（server 自己注册） |
| 负载均衡 | client 各自算 | ✅ 全局视图 |
| 判死 | 各自维护 | ✅ 集中 |
| 代价 | 无 | ⚠️ 需要一个**固定地址的单点** |

**所以 master 的"固定性"不是随意规定，而是架构的必然代价。** 生产系统通常用 HA（主备 + ZooKeeper 选主）解决；**本项目不需要做 HA**（可砍掉的复杂度，设计文档中说明理由）。

---

## 8. 一次闲时压缩的完整时序

把上面所有概念串成一条线：

```
[集群闲时]
  ① scheduler 拉起 compress_server 进程（数量不定）
  ② server 启动 → 采集本机 CPU/MEM/队列 → 主动向 master 注册
  ③ server 周期心跳上报（含最新负载）→ master 更新表
     （此后循环 ③，直到被杀）

[压缩任务触发]
  ④ datanode 内的 compress_client 扫描线程：找出 7 天无访问的数据
  ⑤ client → master: get_workers_info（约每 30s 一次，之后走缓存）
  ⑥ master 返回可用 server 列表（可能已按负载排序）
  ⑦ client 选定 server → 直连发送数据（大 payload，RPC 附件）
  ⑧ server 入队 → 多线程处理 → 压缩（LZ4 或 LZMA）
  ⑨ 若队列过深/busy → 反压 → client 退避 → 重选其他 server 重试
  ⑩ server 压缩完成 → response 回传（附件，含 orig_len / compress_len）
  ⑪ client 收到 → 把压缩后数据写回存储，替换原数据

[被抢占 / 忙时]
  ⑫ scheduler 杀掉 server
  ⑬ 心跳停止 → 超过阈值 → master 判死 → 从列表移除
  ⑭ client 若正在等它的响应 → 超时 → 退避 → 重试别的 server
```

---

## 9. server 的注册与心跳（两个不同动作）

| 动作 | 时机 | 内容 | 性质 |
|---|---|---|---|
| **① 启动注册** | 进程启动时，**一次** | "我是谁、我在哪"（ID、IP、port） | 一次性 |
| **② 周期心跳** | 之后**每 N 秒**重复 | "我还活着" + 最新负载（CPU/MEM/队列） | 持续 |

**为什么必须分开**：
- **注册**解决"发现问题"——master 得先知道存在这么一个 server
- **心跳**解决"存活 + 状态问题"——master 得知道它**现在还活着**、**现在有多忙**

图注印证：server 侧写有 `1. 心跳上报　2. 物理信息采集　3. 队列长度采集　4. 采集信息上报`——`2/3/4` 是**采集流水线**，说明心跳**不是"我还活着"一个比特**，而是**心跳 + 状态快照**二合一。

**推论（重要）**：因为 `last_heartbeat_ts` 每次都刷新，"判死"依据是**最后一次心跳距今多久**。这意味着：
- server 活着但**卡住了**（死锁、GC、长阻塞）→ 心跳停 → **会被误判为死亡**
- 这正是阈值要留余量（3~5 倍心跳间隔）的原因，也是要区分"进程在但无响应"与"进程没了"的原因

**判死后重注册**：server 被 kill 后重新拉起是**一个全新进程**（可能新 IP/端口），所以是**重新注册成一条新记录**，不是"复活旧记录"。这要求 server 启动时**无条件先注册**，不依赖任何本地持久化状态——再次印证"server 无状态"。

---

## 10. master 的行为特征：**reactive，但有决策权**

按图，master **没有任何出向箭头**给 client 或 server——它是 reactive 的（**从不主动推送**）。但要做精细区分：

| 维度 | master 是主动还是被动？ |
|---|---|
| **通信方向** | **被动**：只回应 client 查询、只接收 server 上报 |
| **决策权** | **可以主动**：client 来问时可**当场算出推荐**（排序/指定），也可只返回原始列表让 client 自己选 |

这正是导师课上说的：

> "当然这个也我们当时是 master 做的决策，那这个其实都行，比方说客户端做决策也行，反正，这不影响。"

**真正的主动行为只有一个半**：
1. **超时扫描线程**（若采用主动扫描方案）——时间驱动，算半个主动
2. **判死后的清理**——由上面触发

除此之外**没有**：master 不会主动通知 client"某 server 死了"，也不会主动命令 server 做任何事。

### 判死的两种实现选择

| 方式 | 做法 | 特点 |
|---|---|---|
| **(a) 主动扫描** | master 起定时线程，每 N 秒扫一遍表 | 判死及时；便于输出监控指标 |
| **(b) 惰性判定** | 不扫描，client 查询时才过滤过期项 | 实现最简单；判死有延迟（但延迟无害） |

**阈值工程细节**：`timeout` 必须**远大于**心跳间隔，通常 **3~5 倍**，容忍网络抖动与调度延迟。
- 太敏感 → 误判（繁忙 server 心跳稍慢就被踢）；**代价很小**：下次心跳会自动重新注册
- 太宽松 → 死掉的 server 还在列表里，client 白白浪费一次重试

### 注册表结构（master 内存中）

```
┌───────────┬──────┬──────┬──────┬──────┬───────────┬──────────────────┐
│ server_id │ IP   │ port │ cpu  │ mem  │ queue_len │ last_heartbeat_ts │
├───────────┼──────┼──────┼──────┼──────┼───────────┼──────────────────┤
│ srv-001   │ ...  │ 9001 │ 30%  │ 45%  │ 12        │ 12:00:03         │
│ srv-002   │ ...  │ 9002 │ 85%  │ 70%  │ 240       │ 12:00:01         │
└───────────┴──────┴──────┴──────┴──────┴───────────┴──────────────────┘
```

---

## 11. 选 server 的决策位置（master vs client）

从小到大三种形态：

| 形态 | master 返回什么 | client 做什么 |
|---|---|---|
| **(a) master 直接指定** | "去 srv-003" | 照着发 |
| **(b) master 排序** | **按负载排好序的列表** | pick top |
| **(c) master 只给原始列表** | **原始列表 + 各自负载** | 自己实现选路算法 |

**权衡**：

| 维度 | master 决策（集中式） | client 决策（去中心化） |
|---|---|---|
| 实现位置 | 一处 | N 处（每个 datanode） |
| 一致性 | ✅ 全局视图，不会撞车 | ⚠️ 各自基于过期快照，可能撞车 |
| master 负载 | ⚠️ 每次查询都要算 → 热点 | ✅ 只存表、轻 |
| 算法升级 | ✅ 改一处生效 | ⚠️ 要升级所有 `.so`（**重启 datanode！**） |
| 灵活性 | ⚠️ 固定 | ✅ 可结合本地信息 |
| 可观测性 | ✅ 集中，好调试 | ⚠️ 分散，难排查 |
| 故障影响 | ⚠️ master 挂 → 无法选路 | ✅ 有缓存还能撑一会儿 |

**关键洞察 1：负载均衡与选路是两件事**
- **负载均衡**：监控全局，决定"谁该多接活"
- **选路**：单个请求"发给谁"

**关键洞察 2：client 缓存让"决策权"变得微妙**
client 每 ~30 秒才同步一次，所以**即使 master 做了决策，那也是"30 秒前的对"**。因此**无论决策放哪，都必然配合运行时反馈**（反压 → 退避 → 换 server）。这说明：

> `get_workers_info` 返回的**不是权威指令，而是"推荐/提示"（hint）**。真正的权威信号是 **server 的实时反压**。

**关键洞察 3：导师"这不影响"的含义**
指**架构上不影响**（不改组件职责、不改通信拓扑）；但**工程上有影响**——实习项目放 master 更简单（集中调试、升级方便）；真实系统中若 master 成热点，就要把决策下沉到 client。

**倾向方案（待定）**：`master 提供全局视图 + 推荐排序`，`client 做最终 pick + 运行时重试`。理由：符合图上 master 作为"视图提供者"的职责、接口简单、保留灵活性、便于单测与 mock。

### 排序依据（score）候选

| 候选 | 优缺点 |
|---|---|
| 队列长度最小 | 最简单直观，直接反映"要等多久" |
| `(1-cpu%) + (1-mem%)` | 反映机器负载，但忽略排队 |
| 加权（队列权重最高） | 更贴近真实 |
| 轮询 round-robin | 最公平但忽略负载，仅适合均质 server |

导师课上多次提"队列"，暗示**队列长度是重要指标**。倾向：**队列为主、CPU/MEM 为辅**。

**一致性细节**：排序时应**先剔除判死的 server**，或对心跳较旧的降权——否则会把"即将超时"的排在前面，白白浪费 client 一次重试。

---

## 12. 需要向导师澄清的问题

完整清单见 `.scratch/week1-background-and-design/issues/05-mentor-clarifications.md`。与本文件直接相关的：

| # | 问题 |
|---|---|
| 8 | **"7 天无访问"的 xtime 从哪来？**（HDFS atime / 业务访问日志 / 独立 xtime 服务）——HDFS 默认 `dfs.namenode.accesstime.precision` 为 1 小时，且很多生产集群为性能**直接关闭 atime** |
| 9 | **存储集群与离线集群网络是否互通？**带宽 / 延迟约束？生产环境通常物理隔离，而压缩数据是 GB 级大流量 |
| 10 | master 的注册表**是否需要持久化**？重启后如何恢复？ |
| 11 | **谁指示 scheduler 拉起 compress_server？**（master / 独立控制器 / 常驻服务作业 / 定时任务）——**图上没有画出 scheduler 与 master 的任何连线** |
| 12 | 心跳间隔与判死超时阈值的**具体数值**？ |
| 13 | 判死采用**主动扫描**还是**惰性过滤**？ |
| 14 | 选 server 的**决策者**是谁（master 排序 / master 指定 / client 自选）？ |
| 15 | master 注册表是否需要持久化？（同 10） |
| 16 | **注册与首次心跳是合并还是分开？** |
| 17 | 决策位置（同 14） |
| 18 | 排序依据的 **score 算法**？ |
| 19 | 原系统 datanode 是否为 **C++ 自研**？（"（HDFS）"是类比还是字面） |

---

## 13. 小结：你真正需要"学"的外部技术

| 类别 | 清单 | 需要程度 |
|---|---|---|
| **既有系统** | HDFS、MR/Spark、调度器 | **概念级理解**（能讲清是什么、为什么有忙闲、怎么对接） |
| **第三方库** | BRPC + Protobuf、LZ4、LZMA、CMake | **真正要用**（见后续 `docs/research/dependency-libraries.md`） |
| **自己造的** | compress_client(lib)、compress_master、compress_server、mock 层 | **全部工作量在这里** |

> **一句话记住**：两个集群都是"环境"，不是"工具"。我们从存储集群**读写数据**，在离线集群**借住算力**。

---

## 参考

- 导师手绘架构图：`docs/compress-system.drawio`（唯一权威依据）
- 第 1 课会议记录：`meeting/见识星球 昕祺 软件研发实习第1课.md`
- 精炼版上下文：`GLOSSARY.md`
- mock 决策：`docs/adr/0001-local-mock-external-clusters.md`
- [Hadoop libhdfs（C API）](https://hadoop.apache.org/docs/stable/hadoop-project-dist/hadoop-hdfs/LibHdfs.html)
- [YARN ResourceManager REST API](https://hadoop.apache.org/docs/stable/hadoop-yarn/hadoop-yarn-site/ResourceManagerRest.html)
