# 05: Master 模块——注册表 / 判死扫描 / 查询下发

**What to build:** 一个可独立演示的 master 二进制：server 实例注册后出现在查询结果里，心跳持续刷新其存活状态，心跳超时的实例在判死阈值内从查询结果中消失；查询永远原样返回当前注册表全量（不排序、不指定——选点由 client 随机自选，这是设计文档已定稿的职责划分）。注册表不持久化，master 重启后依靠各实例下一次心跳自动重建。

覆盖三层：注册表逻辑（注册/刷新/查询/判死移除）、判死扫描线程（本地周期遍历时间戳，不发网络请求）、BRPC 服务端三个 RPC（RegisterWorker / Heartbeat / GetWorkersInfo）。

负载字段（queue_len/cpu/mem）随心跳入表并随列表下发，当前选点不使用，仅为策略演进保留。负载感知选点、排序、HA、持久化一律不做（设计文档 §3.2 已定稿）。

**Blocked by:** 04: 文件头 16B——代码侧同步（契约冻结先行）

**Status:** ready-for-agent

- [x] registry 单测覆盖 注册→心跳刷新→查询→判死移除 四态流转
- [ ] 判死扫描单测：构造超时条目，扫描后消失；未超时条目保留
- [ ] GetWorkersInfo 从真 BRPC 查询返回含已注册实例的列表
- [ ] master 进程 Ctrl-C 优雅退出，BRPC 句柄生命周期合规（controller 不得提前释放）
- [ ] `ctest` 全绿，`-Wall -Wextra` 零警告
