# 05: Master 模块——注册表 / 判死扫描 / 查询下发

**What to build:** 一个可独立演示的 master 二进制：server 实例注册后出现在查询结果里，心跳持续刷新其存活状态，心跳超时的实例在判死阈值内从查询结果中消失；查询永远原样返回当前注册表全量（不排序、不指定——选点由 client 随机自选，这是设计文档已定稿的职责划分）。注册表不持久化，master 重启后依靠各实例下一次心跳自动重建。

覆盖三层：注册表逻辑（注册/刷新/查询/判死移除）、判死扫描线程（本地周期遍历时间戳，不发网络请求）、BRPC 服务端三个 RPC（RegisterWorker / Heartbeat / GetWorkersInfo）。

负载字段（queue_len/cpu/mem）随心跳入表并随列表下发，当前选点不使用，仅为策略演进保留。负载感知选点、排序、HA、持久化一律不做（设计文档 §3.2 已定稿）。

**Blocked by:** 04: 文件头 16B——代码侧同步（契约冻结先行）

**Status:** ready-for-agent

- [x] registry 单测覆盖 注册→心跳刷新→查询→判死移除 四态流转
- [x] 判死扫描单测：构造超时条目，扫描后消失；未超时条目保留（另有 Stop 及时性与并发压力用例）
- [x] GetWorkersInfo 从真 BRPC 查询返回含已注册实例的列表
- [x] master 进程 Ctrl-C 优雅退出，BRPC 句柄生命周期合规（controller 不得提前释放）
- [x] `ctest` 全绿，`-Wall -Wextra` 零警告

**同步义务（遗留记录）：**
- ~~`max_concurrency`（查询 64 / 心跳 256）为算账推定值，联调期按 storm 实测标定后回填注释。~~ **已标定（2026-11-25，票 07 storm 格）**：300 渐起/300 爆发/1000 爆发三档零失败、CPU profile 平，在途并发远低于上限，64/256 维持，代码注释已回填实测依据（master_runtime.cpp）。
- proto `WorkerInfo` 字段与 `server_registry.h` 内部结构为手工同步，proto 变更时必须同步更新 registry/service 两侧。

**已知构建约束（欠账）：** TSAN 构建不得链接 glog——brpc 以 WITH_GLOG=OFF 构建（内嵌 butil 注册 `-v`），与系统 glog 的 `-v` 双重注册导致 main 之前 abort；已实证不含 glog 的 TSAN 构建可正常运行（registry/scanner 测试零报告）。长期解法二选一：brpc 重装为 WITH_GLOG=ON，或为纯 std 组件建立不含 BRPC 链接的最小 TSAN 测试 target。

**TSAN 环境级失效（2026-11-24 四测试实测，CP4/CP5 reviewer 复核）：** registry 0 races、death_scanner 1 race（同属 brpc 帧）、server_compress 52、heartbeat_reporter 132——全部 race 堆栈落在 `bthread::TaskGroup::ready_to_run_remote` / `TaskControl::create_group` / `brpc::Acceptor` 等框架内部，项目并发原语（shared_mutex、WorkerPool、Loop、wake_mtx_）零出现在堆栈。根因：bthread 为用户态协程切换，不经过 TSAN 可拦截的 pthread 原语，框架内部 relaxed 原子/自旋交接被记为 race（假阳性），真实同步边丢失（假阴性）。结论：链接 BRPC 后 TSAN 对项目并发件不再有判别力（噪声比 52:0），BRPC 链路并发正确性的验证依赖改为：压测连跑 + 锁纪律代码审查。TSAN 判别力只对纯 std 组件（registry/scanner/worker_pool 纯逻辑）有效，此后不再对 BRPC 链路要求 TSAN 通道。

**设计挑战裁决（PR #2 评审，2026-11-25）：**
- [Challenge-1] HealthyWorkers TTL 谓词过滤 vs 扫描摘除双轨 → **保留双轨，回填设计文档**。理由：scanner 线程若静默停顿，查询路径谓词仍挡住超 TTL 陈旧条目，master 正确性不押注在扫描线程存活上；成本近零（查询本就要 O(n) 拷贝）。§3.2/§7.2 已改述为"扫描摘除 + 谓词双保险"，原"不二次过滤"明确为"不做选点式筛选/排序"。
- [Challenge-2] MaxConcurrencyOf 时序 → **已修**：挪至 AddService 之后、Start 之前（brpc 双重契约：方法须已注册 + 只能在 Start 前调用，server.h:615）。
