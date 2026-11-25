# 07: Master-Server 多实例联调脚本——注册 / 判死 / 列表流转

**What to build:** 一个一键运行的烟测脚本，端到端验证 Master-Server 双方联调（设计文档 §6.4：调度系统直接杀死实例是常态路径，非异常）：

1. 启动 1 个 master；
2. 拉起 N 个 server 实例（N 可配，默认 ≥3，各占不同端口）；
3. 查询断言：列表含全部 N 个实例；
4. 强杀其中 1~2 个实例（模拟调度系统回收，非优雅退出）；
5. 等待判死阈值后再次查询，列表同步收缩；
6. trap 兜底清理全部进程。

Client 侧仅测试 harness（查询断言用），不含任何业务逻辑——Client 模块归 W5。

**Blocked by:** 05: Master 模块；06: Server 模块

**Status:** ready-for-agent

- [ ] 脚本一键跑通：起 master → 起 N server → 列表断言 → 强杀实例 → 列表收缩断言 → 清理
- [ ] storm 格：单进程 storm-client 模拟 N=300 虚拟实例并发心跳，/vars 心跳 QPS 曲线、/status RSS 增量、/hotspots/contention 锁竞争均为预期量级（QPS ≈ N/周期，RSS 增量 MB 级），实测数据记录进排查报告
- [ ] 连接层峰值：风暴后 FD 数量与 /connections 无泄漏，残留清零（pidfile + trap）
- [ ] 判死收缩在阈值时间内完成（≤ 心跳周期 × 倍数 + 扫描周期）
- [ ] 重复执行脚本无残留进程/端口占用
- [ ] harness 测试进 ctest，全绿

**验证依赖注记（2026-11-24）：** BRPC 链路并发正确性验证 = 压测连跑 + 锁纪律代码审查（TSAN 对 bthread 协程切换无判别力，见票 05 欠账节）；storm 场景不退避（固定 fallback 周期，算账见票 06 尾注）。
