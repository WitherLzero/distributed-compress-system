# 08: atomic_replacer + §7.2 类声明骨架

**Parent**: 03 — 工程脚手架与公共/基础类

**What to build**: 两件"结构先于逻辑"的事。其一，压缩结果落盘的唯一替换通道——临时文件 + 原子 rename 的替换器（ADR-0003 纪律：全系统只此一处执行替换，其余代码不直接 rename）。其二，把设计文档 §7.2 的全部公共类（client 侧核心/扫描器/列表同步器/执行器/RPC 桩/远程访问器/元信息视图，master 侧注册表/判死扫描器，server 侧服务实现/心跳上报/工作线程池）落成声明级骨架：类名、职责注释、公开接口签名，成员函数空体——零业务逻辑预填，接口形状先行冻结，供 W4/W5 填肉。

**Blocked by**: 04（构建链与测试基建）。

**Status**: ready-for-agent

- [x] 替换器：临时文件路径生成 → 写毕 → 原子 rename 替换目标；失败路径不留半成品（临时文件清理策略明确）
- [x] §7.2 类清单全部落声明骨架，每类职责注释一行；公开接口签名与设计文档一致（含 C++ 客户端接口四纪律：无异常出口、无全局配置注册、读语义对齐系统调用、零静态初始化）
- [x] 全库编译通过，`-Wall -Wextra` 零警告；类之间只用 include/前向声明，不引入实现级耦合
- [x] 无任何业务逻辑预填（空体/`= default`），评审可见 diff 中无逻辑代码

## 评审豁免与报备（双轴评审 handoff，2026-11-10）

**A 组修复（已随本检查点修复，回归用例锁定）**
- A1 `Commit()` 短路求值 fd 泄漏：fsync 失败分支不再自行清 `fd_`，统一由 `Abort()` 收口（close+unlink）
- A2 `Open()` 不复位 `committed_`：成功 Commit 后重开新一轮再放弃时，析构误判已提交而泄漏新临时文件；修法 = `Open()` 内复位 + 回归用例 `ReopenAfterCommitCleansUp`
- A3 `ClientCore::Assemble` 补 `noexcept`：与 Start/Shutdown/Read 及头注释自引的 ADR-0005「无异常出口」自洽；§7.2 原文回写挂票 09

**B1 单测越界豁免**：issue 03 Out of scope 明文「atomic_replacer 单测（IO 类，W5 联调验证）」。本票新增 `tests/atomic_replacer_test.cc` 系学员知情拍板（2026-11-10：主 agent 误引 issue 03 措辞在先，学员知悉实际票面后仍决定补测）——单测收益 > 票面边界，越界留痕，issue 03 原文不改。

**报备（不改代码，防重议）**
- C2 client 内部实现类暂置 `include/compress/`：与 §6.2「宿主唯一可见 client.h（Pimpl）」方向相悖，骨架期无害，W4 Client 门面期收敛
- C3 `CompressServiceRpcStub` 在设计文档 §7.2 无直接出处，且经指针参数提前冻结 W5 才定稿的 `IRpcAdapter`（前向声明）：保留理由 = 调用面形状先行冻结、头注释已写明接缝角色
- B2 `Result Assemble` 以 `CompressErr` 补位：Result 属已定决策（ADR-0005 引用），W4 Client 门面期代码收敛回 Result
- B3 `Submit(Task&&)` 冻结为 `Submit(CompressRequest&&)`：Task 属未定占位，接受实现具体化，§7.2 回写挂票 09
