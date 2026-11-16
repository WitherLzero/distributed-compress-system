# 06: Server 模块——注册心跳 / 真压缩端点 / 线程池

**What to build:** 一个可独立演示的 server 二进制：启动后向 master 注册并周期心跳，随后接受一条真实压缩请求——attachment 进、LZ4 压缩结果 + compressed_len 出（codec 已完成，直接接新契约，不留占位）。队列满时入队即拒绝（ELIMIT），由 client 退避改投消化；服务端无条件返回压缩结果，收益校验在 client 侧（§5.4）。

优雅退出：先停收新请求，再终止在途任务并反馈失败——client 超时后退避改投，属于尽力而为路径。

**Blocked by:** 03: codec 接口契约修订；04: 文件头 16B——代码侧同步（两者均需先冻结，避免二次返工）

**Status:** ready-for-agent

- [ ] 对真 master 注册 + 心跳：master 列表可见本实例，心跳持续刷新
- [ ] 一条真实 4MB attachment 进 → LZ4 压缩结果出，compressed_len 正确（round-trip 单测背书）
- [ ] 队列满时入队即拒绝，返回 ELIMIT 错误码
- [ ] 优雅退出不挂死，BRPC controller 等句柄生命周期合规并在代码注释注明
- [ ] `ctest` 全绿，`-Wall -Wextra` 零警告
