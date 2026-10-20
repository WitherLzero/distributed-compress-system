# 依赖库调研

> W1 交付物——《技能调研文档》。本文件**持续增补**，当前已完成 **A. BRPC + Protobuf（第一版）**。
> issue：`.scratch/week1-background-and-design/issues/02-dependency-libraries.md`
> 精炼版上下文见 `CONTEXT.md`；外部系统见 `docs/research/external-systems.md`。

## 状态

| 章节 | 状态 |
|---|---|
| A. BRPC + Protobuf | 🔄 第一版已整理（本文件当前内容） |
| B. CMake（工程组织） | ⬜ 未开始 |
| C. 压缩库（LZ4 / LZMA） | ⬜ 未开始 |
| D. 概念补充 | ✅ 已在 `external-systems.md` 覆盖 |

---

# A. BRPC + Protobuf

## A.0 从 UE RPC 切入：需要补的是什么

UE 的 RPC 已经覆盖了"RPC 本质"的认知，脱离引擎后需要自己搭起来的是：

| | UE RPC | 通用 RPC（BRPC） |
|---|---|---|
| 交互对象 | Actor 对象函数（`Server_`/`Client_`/`NetMulticast_`） | **Service**（一组方法） |
| 接口描述 | C++ 反射 + `UFUNCTION` 宏 | **`.proto` 文件**（IDL，语言中立） |
| 序列化 | 引擎自动 | **Protobuf** 自动生成 |
| 网络 | 引擎网络层 / 复制系统 | BRPC 协议栈 |
| 绑定 | 绑死在 UE 引擎 | **独立库**，任意 C++ 程序可用 |

---

## A.1 Protobuf / proto 是什么

### 两个身份，别混

| 身份 | 是什么 | 类比 |
|---|---|---|
| **① IDL（接口定义语言）** | 中立语言描述"消息长什么样、服务有哪些方法" | 一份**契约** |
| **② 序列化格式** | 对象 ↔ 紧凑二进制 | 类似 JSON，但更小更快 |

### 产物

写 `.proto` → 跑 `protoc` → 生成 `.pb.h/.pb.cc`，内含：

| 生成物 | 用途 | 谁写 |
|---|---|---|
| `XxxRequest` / `XxxResponse` 类 | 消息对象（含 get/set/序列化） | 自动 |
| `XxxService` **抽象基类** | **服务端继承并实现** | 你写实现 |
| `XxxService_Stub` 类 | **客户端用来发起调用** | 自动 |
| 序列化/反序列化代码 | `SerializeToString()` / `ParseFromString()` | 自动 |

关键：`.proto` 里必须有 `option cc_generic_services = true;`，`protoc` 才会生成 C++ Service/Stub 基类。

### 为什么不用裸 struct / JSON

| 方案 | 问题 |
|---|---|
| 裸 C++ struct | 无法跨进程/跨语言；内存布局受编译器影响；无版本演进能力 |
| JSON | 文本体积大、解析慢、类型弱；大规模 RPC 不划算 |
| **Protobuf** | 二进制紧凑、编解码快、跨语言、**向后兼容** |

### 精髓：**字段号（field number）才是身份**

```protobuf
message Foo {
    required string user_name = 1;   // ← "1" 是字段号，不是默认值
    optional int32  age       = 2;
}
```

**线上传的只有"字段号 + 值"，没有字段名。**

- ✅ **改名安全**：`user_name` → `username`，线上二进制不变
- ❌ **改号危险**：`=1` 改成 `=3` → 新旧程序解析错乱
- ✅ **加字段安全**：用新号，老程序忽略未知字段
- ✅ **删字段**：不能删号，须 `reserved`，防将来复用出错

> **字段名给人看，字段号给机器认。**

### 修饰符

| 修饰符 | 含义 | 注意 |
|---|---|---|
| `required` | 必须有值，缺了**整条 RPC 失败** | ⚠️ 难演进，谨慎使用 |
| `optional` | 可选，含"是否设置"语义 | 演进友好 |
| `repeated` | 数组/列表 | 用于 workers 列表 |

> BRPC 文档原话："If any **required** field is not set, the RPC will fail." —— 自己的 proto 要少用 `required`，否则接口无法演进。

---

## A.2 BRPC 服务端模型

### Service 实现签名（四个参数最易懵）

```cpp
class MyCompressService : public CompressService {
public:
    void Compress(google::protobuf::RpcController* cntl_base,
                  const CompressRequest*  request,
                  CompressResponse*       response,
                  google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);    // ★ RAII：退出时自动 done->Run()
        brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
        // 业务逻辑...
        response->set_ok(true);
    }
};
```

| 参数 | 是什么 | 类比 |
|---|---|---|
| `cntl` | Controller：不属于请求/响应本身的一切——超时、错误码、**附件**、远端地址 | 信封上的信息 |
| `request` | 客户端发来的消息（只读） | 信的内容 |
| `response` | 要填的回复 | 回信内容 |
| `done` | Closure：回信后续动作（序列化、发回、清理）。**必须且只能调一次** | 寄信后的动作 |

> `done` 为何要手动调：BRPC 支持**异步服务**——可以把 `done` 存起来，等 IO 完成、离开 `CallMethod` 之后再调。`ClosureGuard` 用 RAII 保证异常路径也不漏调。

### 启动与停止

```cpp
brpc::Server server;
server.AddService(&svc, brpc::SERVER_DOESNT_OWN_SERVICE);
brpc::ServerOptions options;
options.max_concurrency = 0;   // 0=不限
server.Start(8000, &options);
server.RunUntilAskedToQuit();  // 或 Stop() + Join()
```

- ⚠️ **一个 `brpc::Server` 只能监听一个端口**；要 N 个端口就起 N 个 Server 实例
- **服务不能在 Start 之后增删**（"You cannot add or remove services after the server is started"）
- `Stop()` 不阻塞、`Join()` 阻塞；**先 Stop 全部再统一 Join** 可省总等待时间
- **优雅退出（免费）**：`Stop()` 时**先处理完在途请求**，对新请求立即返回 `ELOGOFF`

---

## A.3 BRPC 客户端模型

```cpp
brpc::Channel channel;
brpc::ChannelOptions options;
channel.Init("127.0.0.1:8000", &options);     // 单机

CompressService_Stub stub(&channel);
brpc::Controller cntl;
CompressRequest req;  /* ...填空... */
CompressResponse resp;
stub.Compress(&cntl, &req, &resp, nullptr);   // 同步调用
if (cntl.Failed()) LOG(ERROR) << cntl.ErrorText();
```

**两条必须记住的线程安全规则**（官方文档）：
- `Channel.Init()` **不是**线程安全的 → 只能由**一个线程**初始化/销毁
- `Channel.CallMethod()` **是**线程安全的 → 一个 Channel 可被**多线程共用**，无需加锁

> 对本项目的意义：client 内**所有工作线程共用一个 Channel**（连 master 的、连 server 的），不要每线程建一个。
>
> 另：`Channel` 可放栈上；`ChannelOptions` 在 `Init()` 后不再被访问，也可放栈上。

**没有 `brpc::Client` 这个类**——客户端侧对象叫 **Channel**。

---

## A.4 ⭐ 附件（Attachment）——本项目技术核心

### 官方定义

> baidu_std and hulu_pbrpc supports attachments which are sent along with messages and set by users **to bypass serialization of protobuf**. From a server's perspective, data set in `Controller.response_attachment()` will be received by the client while `Controller.request_attachment()` contains attachment sent from the client.
>
> **Attachment is not compressed by framework.**

### 用法

```cpp
// ── 客户端发送大 payload ──
cntl.request_attachment().append(raw.data(), raw.size());
stub.Compress(&cntl, &req, &resp, nullptr);

// ── 服务端取 ──
const butil::IOBuf& in = cntl->request_attachment();
std::string raw; in.copy_to(&raw);
// ...压缩...
cntl->response_attachment().append(compressed.data(), compressed.size());

// ── 客户端取回 ──
const butil::IOBuf& out = cntl.response_attachment();
```

### 为什么必须用附件

| 塞进 Protobuf message | 用附件 |
|---|---|
| 大 payload 走 **Protobuf 序列化**（复制+编码） | **完全绕过序列化**，直接内存搬运 |
| 内存经手 ≥2 次 | 一次 |
| 受 Protobuf 自身 size 限制 | 只受 `-max_body_size` 限制 |

### ⚠️ 注意

- **附件不被框架压缩** —— 传输层要省带宽需自己压（见 A.7）
- 附件在 http/h2 下对应 **message body**（`request_attachment()` = POST body）

---

## A.5 🚨 硬约束 1：默认消息上限 64 MB

官方原话：

> To protect servers and clients, when a request received by a server or a response received by a client is too large, the server or client **rejects the message and closes the connection**. The limit is controlled by `-max_body_size` (default `67108864` = **64 MB**).

**我们的数据动辄几百 MB 到 GB → 开箱即用会直接失败并断连。**

| 方案 | 做法 | 代价 |
|---|---|---|
| (a) 抬高 `-max_body_size` | 设为如 2GB | 单条消息占巨大内存，坏请求可吃光内存 |
| **(b) 分片（chunk）** | 切成固定大小块，多次 RPC | 需"分片-组装"协议，复杂度上升；**但顺带解决重试粒度** |
| (c) 限制单次压缩的数据大小 | 超大文件跳过或只压部分 | 简单，但省不到大头的钱 |

> 倾向 **(b)**：同时解决"2GB 请求失败要整体重传"的问题。
> **待向导师确认**：原系统怎么处理大文件？有没有分片？（见 `issues/05` #21）

---

## A.6 🎁 BRPC 自带"反压"——白捡的

官方原话（几乎逐字就是我们的设计）：

> brpc can limit concurrency at server-level and method-level. When number of requests processed simultaneously would exceed the limit, server responds the client with **`brpc::ELIMIT`** directly instead of invoking the service. **A client seeing ELIMIT should retry another server** (by best efforts). This option avoids over-queuing of requests at server-side.
>
> **Why issue error to the client instead of queuing the request?** A server reaching max concurrency does not mean that other servers reach the limit as well. Let client be aware of the error ASAP and **try another server** is a better strategy from a cluster view.

**所以我们不用自己造反压**：

```cpp
brpc::ServerOptions options;
options.max_concurrency = 0;                          // 0=不限；或设具体值
server.MaxConcurrencyOf("...CompressService.Compress") = 10;   // 方法级
options.method_max_concurrency = "auto";              // AutoConcurrencyLimiter 自动调节
```

client 侧：`cntl.ErrorCode() == brpc::ELIMIT` → **退避 → 换 server**。

**附带好处**：
- `ELOGOFF`：server 退出时的"没处理"信号 → client 换 server，**优雅重启不丢流量**
- 有了明确的"没处理"信号，就能把**"确定没做"**和**"不确定"**分开（见 A.9）

> 文档还解释了为何用并发而非 QPS 限流：QPS 是秒级指标，不擅长应对突发；并发数与关键资源（worker/slot 数）直接相关，更能防过排队（Little's law）。

---

## A.7 💡 传输层压缩 vs 存储层压缩（两层，目标不同）

因为 **Attachment 不被框架压缩**，我们可以做两层压缩：

```
待压缩数据 ──LZ4（快，省带宽）──► 网络 ──► server ──LZMA（慢，省空间）──► 存储
              ↑ 传输层压缩                        ↑ 存储层压缩
```

| 层 | 目标 | 选型 | 理由 |
|---|---|---|---|
| **传输层** | **省带宽** | LZ4 | 必须快，不能拖慢传输；跨机房带宽贵 |
| **存储层** | **省空间** | LZMA | 慢无所谓，反正是闲时 |

> ⚠️ 前提：数据**可压缩**。若原始数据已压缩过（如 JPEG），再压会**变大** → 需先判断可压缩性。

---

## A.8 🎁 BRPC 自带服务发现 / 负载均衡

### NamingService（服务发现）

| 形式 | 说明 |
|---|---|
| `bns://<name>` | 百度内网 BNS |
| **`file://<path>`** | **文件里列地址，内容更新自动重载**（适合单测/简单部署） |
| `list://<addr1>,<addr2>` | 直接写在参数里（不能运行时更新） |
| `http(s)://<url>` | 走 DNS |
| `consul://<name>` | Consul 服务发现 |

### LoadBalancer（选路）

| 策略 | 说明 |
|---|---|
| `rr` | 轮询（假定各机同构） |
| `wrr` | 加权轮询（tag 须是 int32 权重） |
| `random` / `wr` | 随机 / 加权随机 |
| **`la`** | **locality-aware，按延迟自适应** |
| `c_murmurhash` / `c_md5` | 一致性哈希 |

### 连接类型

| 类型 | 特点 | 默认用于 |
|---|---|---|
| short | 每请求建连、用完关 | http/1.0 |
| pooled | 池里取空闲连接 | http/1.1 |
| **single** | 一进程对一 server 至多一条连接，可承载多请求 | **baidu_std（BRPC 默认）** |

---

## A.9 ⭐ 关键：我们的 master vs BRPC 内置 LB —— 两个层次

**这不是二选一，层次不同。**

| | **我们的 master** | **BRPC 的 NamingService + LB** |
|---|---|---|
| **层次** | **架构层 / 服务层** | **通信层 / 请求层** |
| **在哪运行** | **独立进程**（需部署、保活） | **在 client 进程内**（库的一部分） |
| **信息来源** | **主动上报**：server 心跳报 CPU/MEM/**队列长度** | **被动测量**：观察每次 RPC 的延迟、错误率 |
| **决策时机** | 每 ~30s 同步一次全局视图 | **每次 RPC 前**现算 |
| **知识范围** | **全局**（所有 server 状态） | **局部**（单 client 视角） |
| **能感知队列深度吗** | ✅ 能（server 直接告诉它） | ❌ 不能（只知"慢"，不知为何慢） |

> **一句话**：**master 是"我知道大家现在多忙"；BRPC LB 是"我感觉这个地址响应快"。**
> 前者是**状态知识**，后者是**行为观测**。

**这解释了为何要自研 master**：**"队列长度"这种内幕信息只有 server 自己知道，必须主动上报**。BRPC LB 拿不到，只能靠延迟间接推断——**等你观察出"慢"，队列已经堆爆了**。要提前止损，必须有 master。

**可叠加方案**：master 返回列表 → 喂给 BRPC 命名服务 → BRPC 用 `la` 在列表内选路。**但需定权威**（关联 `issues/05` #13 决策位置）。

> 倾向：**让 master 主导**，因为它能用上 BRPC 拿不到的"队列长度"信号。

---

## A.10 🚨 硬约束 2：默认 RPC 超时 500ms vs LZMA 100 秒

BRPC 默认值（官方文档）：

```cpp
int32_t connect_timeout_ms;   // 默认 200 ms
int32_t timeout_ms;           // 默认 500 ms   ← 致命
```

我们的压缩耗时：

| 场景 | 数据量 | LZMA @ ~1 MB/s | 默认 500ms 超时 |
|---|---|---|---|
| 深压缩 | 1 MB | ~1 秒 | ❌ 超时 2 次 |
| 深压缩 | 100 MB | **~100 秒** | ❌ 超时 **200 次** |
| 深压缩 | 1 GB | ~1000 秒 | ❌ 超时 2000 次 |

**后果**：client 每 500ms 超时一次 → 退避重试 → 把同一份 100MB 反复发给别的 server → **瞬间打垮集群**（BRPC 文档警告的 "retry storm"）。

**必须做**（设计文档要写）：

1. **动态设置 RPC 超时**：`cntl.set_timeout_ms(...)`，按数据量与压缩级别估算
2. **区分连接超时与 RPC 超时**：连接超时短（200ms 合理），RPC 超时长
3. **用异步 / Semi-synchronous 调用**：别让工作线程干等（否则线程池全堵）
4. **超时估算纳入排队时间**：`数据量÷带宽 + 数据量÷压缩速度 + 队列等待`

> ⚠️ 这是目前看到**最硬的约束**——不改超时，W5 联调必然一片超时。

---

## A.11 ⭐ 超时 ≠ 失败（RPC 语义的根本限制）

BRPC 官方 FAQ 原话：

> **This issue is not solved on RPC layer.** When response returns and being successful, we know the RPC is processed at server-side. When response returns and being rejected, we know the RPC is not processed at server-side. **But when response is not returned, server may or may not process the RPC. If we retry, same request may be processed twice.**

### "没收到响应"涵盖的五种情况（客户端全都无法区分）

| # | 实际情况 | server 处理了吗 | 客户端能知道吗 |
|---|---|---|---|
| 1 | 请求根本没到 server（连接失败） | ❌ | ❌ |
| 2 | 请求到了，还没开始处理 | ❌ | ❌ |
| 3 | 处理完了，响应在回程丢了 | ✅ | ❌ |
| 4 | 处理到一半崩了 | ⚠️ 部分 | ❌ |
| 5 | **处理完了、响应也发了，但客户端提前超时放弃** | ✅ | ❌ |

> **对我们而言，最常见的是第 5 种**——而且主因**不是丢包，是客户端过早放弃**（默认 500ms，见 A.10）。

### 错误码决定"能否安全重试"

| 错误码 | 含义 | 重试安全吗 |
|---|---|---|
| `ELIMIT` | server **拒绝**，没处理 | ✅ 安全，换 server |
| `ELOGOFF` | server 正在退出，**拒绝** | ✅ 安全，换 server |
| `ERPCTIMEDOUT` | **不知道**处理了没 | ⚠️ 危险，且 BRPC **默认不重试** |
| 连接类（`ECONNREFUSED` 等） | 大概率没处理 | ✅ 较安全 |

> 文档：`ERPCTIMEDOUT is never retried, even if allowed by your derived RetryPolicy` —— 这是在保护你。

---

## A.12 ⭐⭐ 副作用在 Client，不在 Server（结构性优势）

### 关键前提：**写回是 client 干的，不是 server**

| 步骤 | 谁做 |
|---|---|
| 读原始数据 | **client**（`storage_->ReadFile()`） |
| 压缩 | **server**（纯计算） |
| **写回压缩后数据** | **client**（`storage_->ReplaceFile()`） |
| 记录元信息（orig_len/compress_len） | **client** |

**证据**：图注 `3. origin len/compress len` 标在 **client 侧**；且压缩数据要"以附件形式**拿回来**"——若 server 能自己写回存储，就不必传回。

**所以 server 是纯函数**：

```
compress(bytes, level) → compressed_bytes      // 不碰存储、无副作用
```

### 由此得到的结论

> 🎁 **压缩天然幂等**：同样输入永远同样输出。重试它对**数据正确性毫无伤害**（只白烧 CPU）。
> 🎁 **server 被 kill 也安全**：因为服务端没有任何可观察状态。

| server 被 kill 的时机 | 服务端副作用 | 重试安全性 |
|---|---|---|
| 压缩中就被 kill | 无 | ✅ 安全 |
| 压完、响应发出后被杀 | 无（数据在 client 手里） | ✅ 安全 |

> **这是结构性优势**：我们没有"服务端状态"，所以**服务端的失败与重试可以完全不考虑正确性，只管效率**——大幅简化异常处理。写进设计文档。

**BRPC 那句通用警告（针对有副作用的服务）对我方要"打折"**：重复执行无害。
**但——这不代表 client 侧安全。**

---

## A.13 💀 风险 1（恶性）：Client 重试重新读盘 → 双重压缩

**根因在 Client，与 RPC 无关**（RPC 早已完整成功）。触发条件：**重试路径重新读盘**。

```
第 1 轮：
  client 读盘 → 原始数据 1 MB（文本）
  → 发给 server → 压成 100 KB
  → 收到响应 → 写回：盘上现在是 100KB 压缩数据 ✅
  → 但【client 自身后续出错 / 上层误判失败】→ 重试整个操作

第 2 轮：
  client 重新读盘 → 读到的已是【100KB 压缩数据】
  → 当成"原始数据"发给 server → 又压一次 → 双重压缩 💀
  → 且 orig_len 仍写 1MB → 解压时炸
```

### 修复：**分层重试**，失败在哪层就在哪层重试

```
❌ 错误：把整个操作当一个大事务重试
   read_from_disk()  ← 重试时又读一次（可能读到已压缩数据）💀
   compress_via_rpc()
   write_back()
   → 任一步失败 → 从头再来

✅ 正确：分层重试
   原始数据 = read_from_disk()          ← 只做一次，留在内存
   loop:                                 ← 只重试 RPC
     rpc_result = compress_via_rpc(原始数据)   ← 复用内存 buffer！
     成功 → break；失败 → 退避 → 换 server → 继续
   loop:                                 ← 只重试写回
     write_back(compressed, expected_version)
```

### 三条硬约束（写进设计文档「异常处理」）

| # | 约束 | 防的是 |
|---|---|---|
| 1 | **读盘只做一次**，原始 buffer 在内存中持有到写回成功 | 双重压缩 |
| 2 | 重试 RPC 时**复用同一份 buffer**，绝不重新读盘 | 双重压缩 |
| 3 | 写回**只重试写回**，绝不重跑压缩 | 双重压缩 + 无谓 CPU |

**双保险**：重试前查元信息——"此文件是否已被压缩？"是则放弃。

---

## A.14 💀 风险 2（恶性）：Lost update（陈旧写入）

**根因在 Client 的读写竞态，与 RPC 无关**。压缩耗时长（几十秒~几分钟）→ **窗口很宽**。

```
t=0     client 读文件 v1（100 MB）
t=0.1   发给 server
t=0.1   ... 压缩中（LZMA 需 100 秒）...
t=50    ⚡ 应用往文件追加 1 行 → 文件变成 v2（100MB + 4KB）
t=100   server 压完 v1，client 写回
        → 盘上 v2 被覆盖成"v1 的压缩版"
        → 【那 4KB 追加数据永久丢失】💀
```

### 修复：**条件写（CAS）** —— 思想同 HTTP 的 `If-Match` / ETag

```cpp
// ❌ 危险
auto data = storage_->ReadFile(path);          // 盲读
auto compressed = Compress(data);
storage_->ReplaceFile(path, compressed);       // 盲写

// ✅ 安全
auto [data, ver] = storage_->ReadFileWithVersion(path);   // 记下版本
auto compressed = Compress(data);
bool ok = storage_->ReplaceFileIfVersion(path, compressed, ver);
//          ↑ 存储端校验：盘上还是这个版本吗？不是 → 拒绝
```

**一个机制同时解决风险 1 和风险 2**：
- 风险 2：版本变了 → 拒绝写入 → 不丢数据
- 风险 1：重复压缩的数据写回时版本已不符 → 也拒绝

**若底层存储不提供版本机制**，退路：
1. 在元信息里自维护版本（随数据一起存）
2. 时间戳 + 校验（较弱）
3. **接受该风险**——💡 注意：我们的目标数据**天然是冷的**（7 天无访问），实际概率低，但**不是零**（第 8 天突然被追加）。可写"低概率但需防御"或"本期接受并记录"。

---

## A.15 ⚠️ 风险 3（良性）：白烧 CPU

**RPC 层唯一真正剩下的问题**。响应丢 / 客户端提前放弃 → 重试到另一 server → 两个 server 压同一份数据。

- 正确性：✅ 无害（纯函数）
- 资源：❌ 浪费；若普遍发生则算力翻倍（retry storm）

**处理手段**：

| 手段 | 做法 | 针对 |
|---|---|---|
| **① 根治：把超时设对** | 动态 `set_timeout_ms()`（见 A.10） | 消除"提前放弃"这个主要成因 |
| **② 服务端早拒绝** | `max_concurrency` → 满则返 `ELIMIT` | client 早知道、早换 server |
| **③ 退避** | 失败后等待再试，**优先换 server** | 避免捶同一个忙 server |
| **④ 在途上限** | 每 client 同时进行的压缩任务数设上限 | 避免单 client 打爆集群 |
| **⑤ 区分错误码** | `ELIMIT`/`ELOGOFF` 安全重试；`ERPCTIMEDOUT` 更保守 | 减少无谓重试 |

> **①是根治**：server 被 kill 时连接**立刻断**（`ECONNRESET`，秒级），不用等超时——**真会走到超时的场景很少**（主要剩网络分区、server 卡死）。

---

## A.16 修正后的分层结论

```
┌─ RPC 层（无法控制，只能适应）────────────────────┐
│  固有性质：超时 ≠ 失败（语义不确定）               │
│  对我方影响：可能重复执行                          │
│  因 server 无副作用 → 降级为【资源浪费】           │
│  处理：把超时设对（根治）+ 退避 + 早拒绝 + 错误码区分 │
└───────────────────────────────────────────────────┘
┌─ Client 层（能控制，必须修）──────────────────────┐
│  💀 风险1：重试重新读盘 → 双重压缩（数据损坏）      │
│     → 修：分层重试 + 复用内存 buffer               │
│  💀 风险2：读-改-写竞态 → lost update（数据丢失）   │
│     → 修：版本号 / CAS                             │
└───────────────────────────────────────────────────┘
```

> **一句话**：RPC 层给我们的是**效率问题**；**正确性问题 100% 出在 client 自己的逻辑上**——好消息是修复位置明确、都在我们手里。

---

## A.17 三个接口的 proto 草案

结合导师图注（`send data` / `response data` / `orgin len` `compress len` / 队列长度）：

```protobuf
syntax = "proto2";
option cc_generic_services = true;

// ═══ 控制面：client → master ═══
message WorkersInfoRequest { optional string client_id = 1; }

message WorkerInfo {
    required string server_id  = 1;
    required string ip         = 2;
    required int32  port       = 3;
    required uint32 cpu_pct    = 4;
    required uint32 mem_pct    = 5;
    required uint32 queue_len  = 6;    // 图注明确提到的"队列长度"
    optional double score      = 7;    // master 算的排序分（决策放 master 时）
    required int64  last_hb_ms = 8;    // 判死用
}
message WorkersInfoResponse { repeated WorkerInfo workers = 1; }   // 已按 score 排序

service MasterService {
    rpc GetWorkersInfo(WorkersInfoRequest) returns (WorkersInfoResponse);
}

// ═══ 控制面：server → master ═══
message HeartbeatRequest {
    required string server_id = 1;
    required int32  port      = 2;
    required uint32 cpu_pct   = 3;
    required uint32 mem_pct   = 4;
    required uint32 queue_len = 5;
}
message HeartbeatResponse {
    required bool  accepted          = 1;   // 拒绝 → 重走注册流程
    optional int32 beat_interval_ms  = 2;   // master 可动态下发间隔
}
service RegistryService {
    rpc Heartbeat(HeartbeatRequest) returns (HeartbeatResponse);
}

// ═══ 数据面：client → server ═══
enum CompressLevel { SHALLOW = 0; DEEP = 1; }   // 7天 / 14天

message CompressRequest {
    required string        file_path  = 1;   // 数据标识
    required uint64        origin_len = 2;   // ★ 图注: orgin len
    required CompressLevel level      = 3;   // 浅压 / 深压
    optional uint64        version    = 4;   // ★ 幂等性用（A.13/A.14）
    // 实际数据 → 走 attachment
}
message CompressResponse {
    required bool   ok             = 1;
    optional uint64 compressed_len = 2;      // ★ 图注: compress len
    optional string err_msg        = 3;
    // 压缩后数据 → 走 attachment 返回
}
service CompressService {
    rpc Compress(CompressRequest) returns (CompressResponse);
}
```

> ⚠️ **`MasterService` 与 `RegistryService` 都跑在 master 进程**——BRPC 一个 Server 一个端口，两个 service 可挂同一端口（不同 service 名）；若要分端口就得起两个 `brpc::Server`。
>
> ⚠️ 谨慎使用 `required`（见 A.1）：它会阻碍接口演进。

---

## A.18 待办 / 待澄清

| # | 事项 | 位置 |
|---|---|---|
| 21 | 原系统如何**处理大文件**？有无分片？（A.5） | `issues/05` |
| 22 | **写回**由谁做、存储端有无版本机制？（A.12/A.14） | `issues/05` |
| 13 | 选 server 决策位置：master 主导 vs BRPC LB（A.9） | `issues/05` |
| — | B. CMake 调研 | ✅ 见下 |
| — | C. 压缩库调研（LZ4 / LZMA API 与原理） | 本文件待补 |

---

# B. CMake（工程组织）

> **结论先行**：本项目**确定使用 CMake**（导师指定；且 BRPC 官方自身也用 CMake，生态与资料量碾压 Blade）。

## B.0 关键认知纠偏

### CMake **不是**构建工具，而是**构建系统的生成器**

```
CMake  →  [生成]  →  Makefile / build.ninja / .sln
                        ↓
                    make / ninja / MSBuild   ← 这些才是真正的构建工具
```

**CMake 的产物是"构建脚本"，它自己不编译任何东西。** 这是最容易误解的一点。

| | 传统做法 | CMake 做法 |
|---|---|---|
| 你写 | 手写 `Makefile` | 写 `CMakeLists.txt` |
| 换平台 | **重写一份** | 同一份，换 generator |
| 换编译器 | 手改一堆变量 | `-DCMAKE_CXX_COMPILER=...` |

> ✅ **推论：不需要学 Makefile。** 它是 CMake 的**输出**，不是输入——就像写 Markdown 不用管渲染出的 HTML。

### 两个阶段

```
① Configure（配置）— 只做一次，或改了 CMakeLists 之后
   cmake -B build -G Ninja
   → 读所有 CMakeLists.txt、探测编译器、找依赖 → 生成 build/ 下构建脚本

② Build（构建）— 天天用
   cmake --build build -j16
   → 实际调用 ninja/make 去编译链接
```

- **`-S` = source 目录，`-B` = build 目录** → "**out-of-source build**"，构建产物不污染源码
- 现代写法 `cmake -B build` 即可（CMake 3.13+）

**Generator（`-G`）** = "生成哪种构建脚本"：

| Generator | 说明 |
|---|---|
| `Unix Makefiles` | 生成 Makefile（Linux 默认） |
| `Ninja` | 生成 `build.ninja` —— **更快，推荐** |
| `Visual Studio 17 2022` | 生成 `.sln`（Windows/MSVC） |

---

## B.1 核心概念：**Target（目标）**

现代 CMake 围绕 **target** 组织。一个 target 就是"要造出来的一个东西"：

| 类型 | 命令 | 产出 | 本项目 |
|---|---|---|---|
| **可执行** | `add_executable` | `.exe` / 无后缀二进制 | `compress_master`、`compress_server`、`mock_datanode`、`mock_scheduler` |
| **库** | `add_library` | `.a` / `.so` / `.dll` | **`compress_client`（动态库）** |
| 自定义 | `add_custom_target` | 任意命令 | 生成 proto 代码 |

### ⭐ 现代 CMake 的精髓：三条命令

```cmake
# ❌ 旧式（全局变量，污染一切）
include_directories(${BRPC_INCLUDE_DIR})
link_libraries(brpc)
add_executable(master master.cpp)
# → 设置"向上传染"，其他目标也被影响，极难维护


# ✅ 现代（target-based，只作用于指定目标）
add_executable(master src/master/main.cpp)
target_include_directories(master PRIVATE ${BRPC_INCLUDE_DIR})   # ① 头文件去哪找
target_link_libraries(master PRIVATE brpc protobuf lz4 lzma)      # ② 链接什么
target_compile_features(master PRIVATE cxx_std_17)                # ③ 需要哪些语言特性
```

> 🔑 **一句话**：现代 CMake 就是"**每个 target 自己声明需要什么**"，而非"全局设一堆变量"。**记住这三条就能读懂 90% 的现代 CMakeLists.txt。**

### `PRIVATE` / `PUBLIC` / `INTERFACE` —— 决定依赖是否传递

| 关键字 | 含义 | 何时用 |
|---|---|---|
| `PRIVATE` | 只有**我自己**用 | 实现细节（如 `.cpp` 里用的库） |
| `PUBLIC` | 我用，**用我的人**也用 | 公开头文件里 `#include` 了它 |
| `INTERFACE` | **我不用**，但用我的人要用 | 纯头文件库 |

> **判断法则**：那个 `#include` 出现在**公开头文件**里 → `PUBLIC`；只在 **`.cpp` 里** → `PRIVATE`。

---

## B.2 本项目第一个 CMakeLists.txt

### 顶层（仓库根）

```cmake
cmake_minimum_required(VERSION 3.16)      # 见 B.5 版本说明
project(distributed-compress-system CXX)

set(CMAKE_CXX_STANDARD 17)                # 全局：C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)       # 不许降级

find_package(Protobuf REQUIRED)
find_package(LZ4 REQUIRED)
find_package(BRPC REQUIRED)               # ← WSL 下可行；Windows 下找不到

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    proto/compress.proto
    proto/master.proto
)
add_library(compress_proto STATIC ${PROTO_SRCS})
target_link_libraries(compress_proto PUBLIC protobuf)
target_include_directories(compress_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src/common)
add_subdirectory(src/client)
add_subdirectory(src/master)
add_subdirectory(src/server)
add_subdirectory(mock)
```

| 行 | 含义 | 类比 |
|---|---|---|
| `cmake_minimum_required` | 声明最低版本 | "需要 Python 3.10+" |
| `project(...)` | 项目声明 | `package.json` 的 name |
| `find_package(X REQUIRED)` | **找到库并定义变量/目标**；找不到即报错 | `import` / `require` |
| `add_library(... STATIC ...)` | 造静态库（把 proto 生成代码打包） | —— |
| `add_subdirectory(x)` | **去 x 目录读它的 CMakeLists.txt** | `import` 一个子模块 |

> ⭐ `add_subdirectory` 是 CMake 的**模块化机制**：每个目录管自己的目标，顶层只管"有哪些目录"。

### 子目录示例：`src/master/CMakeLists.txt`

```cmake
# ① 定义产出
add_executable(compress_master
    main.cpp
    registry.cpp        # 注册表
    heartbeat.cpp       # 心跳与判死
    selector.cpp        # 选 server
)

# ② 依赖
target_link_libraries(compress_master
    PRIVATE
        compress_proto     # proto 生成的代码
        compress_common    # 公共工具
        brpc
        protobuf
)

# ③ 头文件
target_include_directories(compress_master
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
```

**一个 target 的描述永远长这样**：① 我是什么 → ② 我依赖谁 → ③ 头文件在哪。

### 子目录示例：`src/client/CMakeLists.txt`（动态库，写法略不同）

```cmake
# ① SHARED = 动态库（.so）
add_library(compress_client SHARED
    compress_client.cpp
    scanner.cpp          # 扫描线程
    scheduler.cpp        # 选 server + 退避
    compressor_lz4.cpp
    compressor_lzma.cpp
)

# ② 依赖 —— 注意 PUBLIC/PRIVATE 的区别
target_link_libraries(compress_client
    PRIVATE
        brpc
        protobuf
        compress_proto
    PUBLIC
        LZ4::LZ4              # ← 公开头文件里 include 了 lz4
        LibLZMA::LibLZMA
)

# ③ 头文件 —— PUBLIC 因为 mock_datanode 要 include 我的头
target_include_directories(compress_client
    PUBLIC  include/
    PRIVATE src/
)

# ④ lib 形态的工程细节
set_target_properties(compress_client PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    CXX_VISIBILITY_PRESET hidden    # 只导出显式标记的符号，避免污染宿主进程
)
```

### 静态库 vs 动态库（一行之差）

```cmake
add_library(x STATIC ...)   # → libx.a    代码被复制进使用者
add_library(x SHARED ...)   # → libx.so   运行时加载  ★ 我们要这个
```

**选择依据**：client 要以 **lib 形式嵌进 datanode**，且希望独立替换 → **`SHARED`（.so）**。

---

## B.3 本项目会得到的 5 个 target

| Target | 类型 | 来源 |
|---|---|---|
| `compress_client` | **动态库（SHARED）** | `src/client` |
| `compress_master` | 可执行 | `src/master` |
| `compress_server` | 可执行 | `src/server` |
| `mock_datanode` | 可执行 | `mock/datanode`（**实现 `IStorage`**，加载 client） |
| `mock_scheduler` | 可执行 | `mock/scheduler` |

---

## B.4 依赖怎么引入：三种方式

| 方式 | 命令 | 适合 | 本项目 |
|---|---|---|---|
| **① 系统包 / find_package** | `find_package(BRPC REQUIRED)` | 依赖已装在系统里 | ✅ **WSL 下最合适** |
| **② 包管理器**（vcpkg / Conan） | toolchain 文件 + `find_package(... CONFIG)` | 依赖复杂、跨平台一致 | ⚠️ **vcpkg 不支持 Windows 上的 brpc**（`"supports": "!windows"`） |
| **③ FetchContent** | `FetchContent_Declare/MakeAvailable` | 从源码拉、随项目构建 | ⚠️ BRPC 编译很重，会拖慢每次构建 |

**本项目依赖清单**：

```
BRPC ──┬── protobuf   (序列化 + 代码生成)   必需
       ├── gflags      (全局命令行参数)      必需
       ├── leveldb     (仅 /rpcz 追踪需要)   ⚠️ 可选
       └── openssl     (仅 https 需要)       ⚠️ 可选
LZ4     (浅压缩)                            必需
liblzma (深压缩)                            必需
```

> 💡 **BRPC 的 `leveldb` 与 `openssl` 都是可选项**（官方文档明确各自用途）。若不需要 rpcz 追踪与 SSL，**可以砍掉以简化依赖** —— 待与导师确认。

### proto 代码生成接入构建

```cmake
find_package(Protobuf REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    proto/compress.proto
    proto/master.proto)

# 独立的小库 target，其他目标链接它
add_library(compress_proto ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(compress_proto PUBLIC protobuf)
target_include_directories(compress_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
```

> ⚠️ 生成的文件落在 `build/` 目录里，所以必须用 `${CMAKE_CURRENT_BINARY_DIR}` 把它加进 include 路径——**这个变量容易写错**。

---

## B.5 实操注意（4 个易踩的坑）

| # | 事项 | 说明 |
|---|---|---|
| 1 | **CMake 版本下限** | 本机 MSYS2 是 **4.4.2**，而 WSL Ubuntu 20.04（BRPC Dockerfile 的基础镜像）默认是 **3.16**。建议声明 `cmake_minimum_required(VERSION 3.16)`，两边都能跑 |
| 2 | **BRPC 的 find_package 包名** | BRPC 安装后提供的 CMake 配置包名**需实际验证**（可能需要手写 `FindBRPC.cmake`）。**这是 W3 第一个要踩的坑** |
| 3 | **proto 生成路径** | 同 B.4 末尾——`${CMAKE_CURRENT_BINARY_DIR}` |
| 4 | **`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`** | 建议一开始就开——生成 `compile_commands.json`，让 VS Code / clangd 的补全与跳转准确（BRPC 官方也推荐） |

### 日常只需 3 条命令

```bash
cmake -B build -G Ninja        # ① 配置（一次）
cmake --build build -j16       # ② 构建（天天）
cmake -B build                 # ③ 改了 CMakeLists 后重新配置
```

> 不需要记 `make`，不需要读 Makefile。

---

## B.6 Blade 调研（对比结论）

[Blade](https://github.com/blade-build/blade-build) 已演进到 **Blade 3**（比导师当年版本强很多）：

| | **CMake** | **Blade** |
|---|---|---|
| 定位 | **通用**构建系统生成器，事实标准 | **大型 monorepo 专用**（腾讯 3000 万行代码级） |
| 语法 | `CMakeLists.txt`（自家语言，啰嗦） | `BUILD`（**Python 语法**，声明式简洁） |
| 依赖声明 | 手写 `target_link_libraries` | **自动推导间接依赖**（只声明直接依赖） |
| 第三方库 | find_package / vcpkg / Conan | **原生集成 vcpkg**（`vcpkg#port:lib`） |
| 平台 | **全平台** | Blade 3 已支持三平台（有 Windows CI） |
| 后端 | 生成 make/ninja | 生成 **Ninja** 脚本（本身依赖 Ninja + Python 3.10+） |
| 生态 | **极丰富**（资料、AI 可答、社区问答） | **较少**（腾讯/小米/百度/爱奇艺等内部用） |
| 传承 | 事实标准 | 启发自 Google（后来的 Bazel 是 Google 自己的重写版） |

**导师的评论是准确的**：Blade 好用、但流行度不如 CMake、资料更少。

### 结论：用 CMake

理由按重要性：

1. **BRPC 官方自身就用 CMake** —— 依赖方一致，少一层摩擦
2. **生态/资料量碾压** —— "AI 讲清楚就行"这条，Blade 资料明显不足
3. **导师指定用 CMake**
4. **Blade 的强项（monorepo、3000 万行）对我们 5 个 target 是降维打击**，收益为负（还多一条 Python+Ninja 依赖链）

> 📌 **值得记下的权衡点**：Blade 的"**自动推导间接依赖**"确实解决了 CMake 一个真实痛点——CMake 要手写每个 target 链接什么，库多了容易漏。但本项目依赖就那么几个，手工写完全可控。

---

## B.7 前置阻塞：编译环境（见 `external-systems.md` §5）

CMake 本身没问题，但**编译环境存在阻塞**：**BRPC 官方不支持 Windows**（vcpkg port 标注 `"supports": "!windows"`；依赖 epoll）。

- 详细分析、WSL2 与 Docker 对比、Agent 操作 WSL 的可行性 → **`docs/research/external-systems.md` §5**
- 待定项 → `issues/05` #25、#26

---

## 参考

- [BRPC 服务端 Basics](https://brpc.apache.org/docs/server/basics/) —— 附件、消息上限、并发限制、压缩
- [BRPC 客户端 Basics](https://brpc.apache.org/docs/client/basics/) —— Channel、NamingService、LoadBalancer、连接类型、重试策略、附件
- [BRPC getting started](https://brpc.apache.org/docs/en/getting_started.html) —— 依赖清单、支持环境（**无 Windows**）
- [Blade 官方 README](https://github.com/blade-build/blade-build) —— Blade 3 特性与定位
- [vcpkg brpc port](https://github.com/microsoft/vcpkg/blob/master/ports/brpc/vcpkg.json) —— `"supports": "!windows"`
- 导师架构图：`docs/compress-system.drawio`

## Comments

- **2026-10-19**：A 部分第一版整理完成。来源：BRPC 官方文档（server/client Basics 全篇精读）+ 各轮讨论。
  期间修正了一次错误归因（曾把"双重压缩/lost update"误记为 RPC 层问题，实际根因均在 **client**）。
- **2026-10-19**：**B 部分（CMake + Blade）完成**。要点：CMake 是生成器而非构建工具（不必学 Makefile）；现代用法为 target-based 三条命令；本项目 5 个 target；**发现 BRPC 不支持 Windows** 的编译环境阻塞（详见 `external-systems.md` §5）。
  待续：C. 压缩库（LZ4 / LZMA）。
