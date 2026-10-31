# client 对外接口设计：三方案比较（design-it-twice 记录）

> 支撑材料：本文件记录 `compress_client` 对外接口的方案探索过程与落选理由；最终决策见
> `docs/adr/0003-c-abi-client-interface.md`，接口规格见设计文档 §6.2。
> Status: 记录性文档，可随设计演进修补（区别于冻结的 ADR）。
> Date: 2026-10-31 · Decider: 林昕祺（学员）

## 探索对象

`compress_client` 以动态库形式嵌在 datanode 宿主进程内，宿主如何调用库（导出哪几个
C 函数、错误如何传达）在设计文档中原本只有一句"对外接口为 C 语言风格的函数集合"。
本记录以 design-it-twice 方法（对同一接口独立出三个方案再比较）填补该空格。
探索**只针对宿主↔库的 C API 边界**，不涉及库内部类设计、宿主业务逻辑与 proto。

## 共同约束（三方案共享，不可谈判）

动态库的调用方与库是**独立编译**的二进制，C++ 无稳定 ABI（名字修饰规则各编译器不
同、std:: 类型布局随 GCC5 `_GLIBCXX_USE_CXX11_ABI` 等开关变化，编译链接可通过而运
行时内存错乱）。因此对外接口必须为 C ABI，并在边界掐死三类事故：

1. C++ 异常穿越 `extern "C"` 边界是未定义行为（terminate 杀宿主进程）→ 外壳统一捕获转返回码；
2. 内存所有权单向：宿主内存只进不出；
3. 静态初始化收进显式 create（dlopen 期静态对象构造顺序不可控）。

## 方案 A：极简（2 函数）

```c
int cc_start(const char* config_json, cc_handle_t* out);
int cc_stop(cc_handle_t handle);
```

主张：库为黑盒家电，读路径由库在底层拦截宿主 `read()`，对宿主完全透明。

**落选理由**：透明拦截在 C 动态库中做不到——`.so` 无法介入宿主的系统调用（除非
LD_PRELOAD 劫持，属不可接受的 hack）。"2 个入口"依赖一个幻觉缝，读路径能力无处安放。

## 方案 B：常见路径（6 函数）✅ 采纳为主干

```c
ccl_version / ccl_strerror / ccl_create / ccl_start / ccl_read / ccl_destroy
```

- 读路径为**显式** `ccl_read`：透明的是数据内容（已压→解压），不是调用路径；
- 错误词汇表 7 个返回码，每个宿主均可行动（EBUFSMALL 扩容重调 / EBUSY 背压重试 / EIO 记日志）；
- 内部可自动消化的失败（master 不可达、server 拒绝、退避重选、temp 清理）一律不上抛；
- 形态对标 SQLite / leveldb 的 C 绑定，为业界验证数十年的动态库标准形态。

## 方案 C：灵活（~18 函数，5 抽象接口）

为每种"将来可能变的东西"预留缝：IScanPolicy / ICompressor / IWritebackPolicy /
IRpcAdapter / IBackendFileStore，另配扩展工厂、事件回调、可编程假 RPC。

**筛缝判据：缝要有第二种真实实现才成立（two adapters = real seam）。**

| 缝 | 第二种实现存在吗 | 处置 |
|---|---|---|
| ICompressor | 有（算法集会演进） | 保留（codec 内部统一入口） |
| IRpcAdapter | 有（单测需注入假件） | 保留为**纯内部** C++ 接口 |
| IBackendFileStore | 有（mock 环境即第二种实现） | 保留为内部文件抽象 |
| IScanPolicy | 无（冷判定只有 atime 一种） | 否决（假想缝） |
| IWritebackPolicy | 无（回写已定 rename 一种） | 否决（假想缝） |

另采其纪律：元信息文件头（磁盘契约）**刻意不做缝**——磁盘格式出现第二种写法即数据事故。
其回调机制（宿主回调中不得调库 API，否则重入死锁）引入新错误模式，且 18 函数中多数
宿主用不到（接口税），整体不采纳。

## 最终：B 主体 + 定点吸收

| 来源 | 吸收内容 | 理由 |
|---|---|---|
| A | cfg 键值对配置（`ccl_create` 的 cfg 参数） | 增配置项不破坏 ABI |
| C | IRpcAdapter 内部 DI + FakeRpc 测试注入 | 控制面逻辑可单测，不经 C ABI |
| C | MetaHeader 磁盘格式钉死 | 见上 |
| C | 纪律：统计口为下一个真实入口 | 预留 `ccl_stat` 为 v1.1 扩展 |

W3 起步文件树与五类（ClientCore/ColdScanner/CompressTaskQueue/CompressExecutor/
读取路径）到文件的映射，见设计文档 §7.2 及 Hindsight 知识页
`compress-client-c-abi-w2-design-it-twice`。
