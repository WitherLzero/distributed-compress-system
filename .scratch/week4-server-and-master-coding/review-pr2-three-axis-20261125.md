# PR #2 整体评审报告（三轴）—— 2026-11-25

- 评审对象：feat/week4-server-and-master → main（base b5b3db2，head 24fd2df，9 commits，+2480/−296，52 files）
- 执行方：空白评审 agent（三轴并行子代理 + 主线程交叉实证），流程按 handoff §6
- 主线程实证基线：RelWithDebInfo 全量构建 0 warning；ctest 12/12 全绿（6.3s）；ldd 实证 compress_master/compress_server 均不链 libglog（但 server 链接行含 -lglog，见 S-附注）；mock_datanode 链 glog（不链 brpc，无冲突）。
- TSAN：按票 05 欠账节仅对纯 std 组件要求；本轮手工 TSAN 复跑被环境拦截（WSL2 ASLR → "unexpected memory mapping" FATAL，与内容无关），采信票 05 已记录的 2026-11-24 实测（registry 0 races；scanner 1 race 属 brpc 帧）+ 本评审锁纪律代码审查。

## Standards（0 硬违规；7 条判断级）

1. **失效注释**：src/master/master_runtime.h:24 "错误经 glog 输出"——本 PR 已确立 master/server glog-free 走 butil（master/main.cpp:1、infra_glog.cpp 头注），应改 "butil 日志"。
2. **注释与约束自相矛盾**：tests/CMakeLists.txt:24 称 server_registry_test "TSAN 下验证无竞态"，但该 target 链接 BRPC::brpc（:28，经 server_registry.h 的 <bvar/bvar.h> 引入），与票 05 欠账节"链接 BRPC 后 TSAN 无判别力"的决策直接冲突。
3. **Duplicated Code**：PollUntil/WorkersCount 在 heartbeat_reporter_test.cc:33-56 与 server_runtime_test.cc:39-56 逐字重复；master_smoke_test.cc:105-116 第三处同类轮询。可提取测试公共头。
4. **Duplicated Code（弱）**：master_runtime.cpp:23-27 `clock ? clock : RealClockMs()` 三连；可提局部变量。
5. **Speculative Generality（弱/观察）**：options.h:14 temp_dir 本期无消费方（自注 W5 client 消费），有票据背书，列观察。
6. **Primitive Obsession（弱）**：HeartbeatReporter 构造 5 个位置参数（heartbeat_reporter.h:36-39）；LoadSample 裸 double 字面量。
7. **有意重复承认项**：DeathScanner/HeartbeatReporter 停机骨架同构（作者已注"同构"），纪律选择不算违规。

## Spec（1 条冲突须裁决；1 条 partial；票 07 未闭环但账目一致）

1. **(c) 查询路径二次过滤与"原样返回"明文冲突**：设计 §3.2（design-document.md:212/219）与 §7.2（L749）要求"查询原样返回注册表，失效剔除由扫描线程负责，不二次过滤"；实现为 scanner 物理摘除 + HealthyWorkers 惰性 TTL 过滤双轨（server_registry.cpp:20-27，master_service_impl.cpp:29），且 master_service_impl.cpp:28 注释自称"不二次过滤"——代码/注释/spec 三方互斥。行为与 spec 等价（scanner 2s 内摘除），但职责模型已变，须回填设计文档或改回单轨。
2. **(a) ELOGOFF 未实现（partial）**：§5.5（design-document.md:444）定义 ELOGOFF=服务端退出中；停机窗口入队被拒一律回 ELIMIT（compress_service_impl.cpp:24；worker_pool.cpp:27 closed_ 分支与满队列同码），滞留任务回业务 FAILED。client 改投语义相同，但可观测性区分未落地，且 ELOGOFF 全 diff 零出现。
3. **(a) 票 07 未闭环**：spec.md Acceptance 多实例联调未交付，票 07 全 [ ]，PR body 声明"待 merge 后执行"——表述一致无隐瞒，但 week 级验收未完成，须留 W5 账目。

确认无误（handoff §3 盲区逐条）：票 05/06 checklist 与实际交付逐条闭合（4MB round-trip 在 server_compress_test.cc:82-107 背书，含 compressed_len==附件大小）；.tmp 归属纠正已闭环（全树无 SweepTempDir/--temp_dir 残留，options.h:14-18 注释与设计 §3.1.4 L163 一致，票 06 L20 有纠正记录）；beat_interval_ms master 裁定→server 消费→失败固定 fallback（heartbeat_reporter.cpp:73-80）；WorkerInfo 7 字段与 proto §6.1 逐字段对齐（master_service_impl.cpp:30-37，ip 取 remote_side 为唯一合理来源）；双 main 同构（InitFlags+InstallShutdownHandlers 先于 Start，butil-only 日志）；PR body 与 diff 一致（12/12 ctest、client 零改动、无孤儿 SHA 钉点）。

## Industry（1 条硬冲突；4 条判断级）

1. **(a) 硬：MaxConcurrencyOf 调用时序违反 brpc 明文契约**。brpc server.h:615："These interfaces can ONLY be called before the server is started"；master_runtime.cpp:55-56 在 server_.Start()（:45）之后调用。当前版本实测生效（首次请求时读取），但属依赖未文档化实现细节——前移两行即可消除。（本评审已对着 /usr/local/include/brpc/server.h:615 原文复核。）
2. **(b) worker 循环 payload.copy_to 全量连续化**（worker_pool.cpp:81）：IOBuf 零拷贝进、std::string 拷贝出，每 4MB 任务多一次 memcpy；注释已记录权衡（ICodec 收裸指针），建议留 scatter-gather（IOBuf/iovec）演进 TODO，本 PR 不改。
3. **(b) HeartbeatReporter 每实例专属 Channel**（heartbeat_reporter.cpp:20）：票 06 脚注已算账背书（300×5s=60QPS、256 并发兜底）；提示：未来第二种 server→master RPC 应复用此 channel。
4. **(b) 心跳周期协商 = 服务端授期 + 失败 fallback**：etcd lease 变体，比 gRPC keepalive 更贴合被动控制面；符合领域形态，无偏差。
观察（不占额度）：LoadSampler busy 口径 = htop 且有注释背书；WorkerPool::Stop 残余补 Run 注释准确命中 delayed-done 停机悬挂坑；RemoteIp .c_str() 多余拷贝无害。

## 挑战清单（§7 格式，≤5 条，判定权在学员）

- **[Challenge-1] HealthyWorkers 惰性过滤**：查询路径用 TTL 谓词而非"原样返回"。
  - 行业/系统参照：设计文档 §3.2.3/§7.2 明文"不二次过滤"；被动控制面单职责模型（scanner 独占剔除）。
  - 本仓选择：scanner 摘除 + 谓词过滤双轨（server_registry.cpp:20-27）。
  - 质疑：双轨的实际收益是什么（scanner 停顿兜底？假时钟测试便利？）？若有收益，设计文档应改述为"双保险"；若无，应删谓词回归单轨——代码与文档必须合一。
  - 去向：问主 agent——双轨是有意设计还是scanner落地前的过渡形态残留？设计文档哪一句应改？
- **[Challenge-2] MaxConcurrencyOf 在 Start 后调用**：依赖"首次请求时才读取"的未文档化实现细节。
  - 行业参照：brpc server.h:615 明文 "ONLY be called before the server is started"。
  - 本仓选择：master_runtime.cpp:55-56 在 Start 后设置（含算账注释）。
  - 质疑：brpc 版本升级若改为 Start 时快照，64/256 阀门静默失效且无测试能拦——为什么不把两行挪到 Start 前（零成本）？是否有"必须先 Start 才知道方法名"的阻碍？
  - 去向：问主 agent——挪到 Start 前是否打破任何已验证行为；若无，直接修。
- **[Challenge-3] ELOGOFF 缺位**：停机窗口的拒绝与队列满同码 ELIMIT。
  - 行业参照：设计文档 §5.5 错误码表区分 ELIMIT（满）/ELOGOFF（退出中），可重试语义同但运维可观测性不同；brpc 内建支持 ELOGOFF。
  - 本仓选择：TryPush 的 closed_ 与满队列同一 false 出口（worker_pool.cpp:27）。
  - 质疑：§5.5 的 ELOGOFF 行是"文档写了实现可选"的流控条款，还是本期漏实现？client 改投语义相同是否足以把 ELOGOFF 从 §5.5 删掉？
  - 去向：问主 agent——补 ELOGOFF 分支（TryPush 返三分态）还是修订 §5.5 删行。
- **[Challenge-4] codec 输入契约逼出的 4MB memcpy**：IOBuf→std::string 连续化。
  - 行业参照：LZ4F/zstd 均支持流式/分块输入；brpc IOBuf 零拷贝是本 PR 自己声明的收益（压缩端点"IOBuf 零拷贝"）。
  - 本仓选择：worker_pool.cpp:81 每次压缩前 copy_to 全量拷贝（注释已自承权衡）。
  - 质疑：CPU 密集 worker 上 4MB memcpy ≈ 毫秒级常驻开销，与"零拷贝"诉求自相矛盾；ICodec 升级到 IOBuf/iovec 输入的演进项该不该现在立票（W5 client 也要走同一接口）？
  - 去向：问主 agent——W5 前是否值得把 ICodec 输入改成 butil::IOBuf（或加 scatter 重载），还是接受 memcpy 至性能实测说话。

## 合并清单

**合并前必修（2 项，均一行级）：**
1. MaxConcurrencyOf 两行移至 server_.Start() 之前（Industry-1，brpc 明文契约）。
2. master_runtime.h:24 "glog"→"butil 日志"（Standards-1，本 PR 自定纪律的自洽）。

**合并前裁决（2 项，学员判定后可改码或改文档）：**
3. Challenge-1：HealthyWorkers 双轨 ↔ 设计文档"不二次过滤"——改码或回填文档。
4. Challenge-3：ELOGOFF 补实现或 §5.5 删行。

**记账（进 W5/票 07，不挡合并）：**
5. 票 07 多实例联调 + storm 标定（max_concurrency 64/256 回填）——PR body 已声明待 merge 后执行。
6. tests/CMakeLists.txt:24 TSAN 注释与票 05 欠账决策对齐（删"TSAN 下验证"或注"仅纯 std 目标"）。
7. codec scatter-gather 演进评估（Challenge-4）；测试公共件提取（Standards-3）。

*判定权在学员（collaboration-protocol §7.4）：挑战答案落票尾台账/ADR/头注释后，同点下次评审自动合规。*
