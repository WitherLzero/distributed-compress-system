# 06: Server 模块——注册心跳 / 真压缩端点 / 线程池

**What to build:** 一个可独立演示的 server 二进制：启动后向 master 注册并周期心跳，随后接受一条真实压缩请求——attachment 进、LZ4 压缩结果 + compressed_len 出（codec 已完成，直接接新契约，不留占位）。队列满时入队即拒绝（ELIMIT），由 client 退避改投消化；服务端无条件返回压缩结果，收益校验在 client 侧（§5.4）。

优雅退出：先停收新请求，再终止在途任务并反馈失败——client 超时后退避改投，属于尽力而为路径。

**Blocked by:** 03: codec 接口契约修订；04: 文件头 16B——代码侧同步（两者均需先冻结，避免二次返工）

**Status:** ready-for-agent

- [x] 对真 master 注册 + 心跳：master 列表可见本实例，心跳持续刷新（heartbeat_reporter_test 三用例：真 MasterRuntime 服务端 + 首跳 500ms 内注册 / 负载字段随跳刷新 / Stop 等待可打断）
- [x] 一条真实 4MB attachment 进 → LZ4 压缩结果出，compressed_len 正确（round-trip 单测背书）
- [x] 队列满时入队即拒绝，返回 ELIMIT 错误码
- [ ] 优雅退出不挂死，BRPC controller 等句柄生命周期合规并在代码注释注明
- [x] `ctest` 全绿，`-Wall -Wextra` 零警告
**同步义务（遗留记录）：**
- WorkerPool 队列容量默认值与内存预算公式（§3.4）尚未落地：峰值内存 ≈ queue_capacity × 平均 payload + worker_count × (payload + out 缓冲 ≈ payload×1.125)。CP6 装配票（ServerRuntime 接 Options）时，worker/queue 默认值必须带这条算账注释（格式沿用 MaxConcurrencyOf 先例），公式先于数字。
- 心跳失败退避决策：维持固定 fallback 周期（默认 5s），不引入指数退避。算账：master 重启窗口秒级，最坏 N×几跳空转，300 台 × 5s = 60 QPS 已由 master max_concurrency=256 兜底；指数退避+jitter 属 10^4~10^5 台规模技术（gRPC/etcd 连接治理），本期过度设计。首跳失败后 Loop 按 fallback 周期重试（无紧贴即时重试），心跳 RPC 超时 2s 封顶单次失败。issue 07 storm 场景不退避。

- [ ] 启动清扫 `.tmp` 残留（评审 Challenge-1 拍板，2026-11-16）：datanode 宿主启动序列中、注册 BRPC 服务之前，清空 `Options::temp_dir` 目录内容（只删文件，保留目录本体）；带预埋残留的单测（预埋 → 启动 → 目录空）。依据：设计文档 §3.1.4 启动清扫要求 + HDFS datanode initStorage block recovery 先例（启动一次性清扫）；Open 时顺手清扫已否决——.tmp 为多压缩线程共享目录，运行中清扫会误删兄弟任务在途半成品，启动时刻无在途任务是唯一安全窗口
