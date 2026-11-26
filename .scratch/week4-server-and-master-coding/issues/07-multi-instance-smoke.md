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

**Status:** completed（2026-11-26：手动+一键联调、storm 三档标定、判死时延实测与重复执行收官）

- [x] 脚本一键跑通：起 master → 起 N server → 列表断言 → 强杀实例 → 列表收缩断言 → 清理
- [x] storm 格：单进程 storm-client 模拟 N=300 虚拟实例并发心跳（渐起+爆发双模式），QPS 精确命中预期（299.8/300.0/1000.0）；锁竞争经 perf cpu-clock 火焰图判读无热点（libbrpc.a 未编 profiler，/hotspots 不可用，以 perf + addr2line 解析链替代），实测数据见下方记录
- [x] 连接层峰值：风暴后 FD 8→8、/connections 清零（2026-11-25 实测）；进程残留由 pidfile + trap 背书
- [x] 判死收缩在阈值时间内完成（≤ 心跳周期 × 倍数 + 扫描周期）
- [x] 重复执行脚本无残留进程/端口占用
- [x] harness 测试进 ctest，全绿

**storm 实测记录（2026-11-25，WSL2）：**

| 模式 | QPS | fail | first-beat avg | max |
|---|---|---|---|---|
| 300 渐起（错相 1s） | 299.8 | 0 | 3.1ms | 65.8ms |
| 300 爆发（同刻齐发） | 300.0 | 0 | 16.9ms | 34.7ms |
| 1000 爆发 | 1000.0 | 0 | 28.4ms | 73.6ms |

- CPU：三档 profile 均平（最大单帧 2.67%）；爆发代价 = 建连排队延迟（等待非计算，CPU 采样不可见），延迟增长亚线性（连接 ×3.3，avg ×1.7）。
- 扩容边界：FD 上限（本机 1M）、accept backlog、注册表拷贝三条悬崖均在 10³ 包线之外；HealthyWorkers 拷贝读未被 storm 压到（storm 只心跳不查询），留 W5 client 联调观察。
- storm-client 连接面：brpc SocketMap 按 {对端, ChannelSignature} 全局共享 socket（默认签名全零），须以 connection_group 逐实例区分才是真实 N 连接（channel.cpp ComputeChannelSignature）。

**验证依赖注记（2026-11-24）：** BRPC 链路并发正确性验证 = 压测连跑 + 锁纪律代码审查（TSAN 对 bthread 协程切换无判别力，见票 05 欠账节）；storm 场景不退避（固定 fallback 周期，算账见票 06 尾注）。

## CP7.3 收官记录（2026-11-26）

**判死时延实测**（快进档：心跳 1s / ttl 2s / 扫描 1s）：kill -9 后以 smoke_query 200ms 轮询，三轮 kill→列表消失 = 1811 / 1799 / 1803 ms。分解 = 距下一次 ttl 到期（kill 发生在注册确认后，末跳约 +0.2~1s）+ 扫描取整，理论上界 ttl+scan=3s 未触及；三轮极差 12ms，判死链路时延稳定。

**重复执行**：标准档（心跳 5s / ttl 20s / 扫描 1s）连跑 3 次全 PASS，进程与监听端口复核零残留。顺手修复脚本 BUILD_DIR 默认值（原默认指向 tests/ 自身，从仓库根直接调用找不到二进制）。
