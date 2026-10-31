# ADR-0003: compress_client 对外接口采用 C ABI 最小函数集

- **Status**: Accepted（2026-10-31 学员定稿；待导师审阅）
- **Date**: 2026-10-31
- **Deciders**: 林昕祺（学员）

## Context

`compress_client` 以动态库形式嵌在 datanode 宿主进程内。宿主与库是**独立编译**的二进制，其交接合约必须稳定：

- C++ 无稳定 ABI：名字修饰规则随编译器不同；std:: 类型布局随编译器开关变化（GCC5 `_GLIBCXX_USE_CXX11_ABI` 事故：`std::string` 布局改变，编译链接通过、运行时内存错乱）；模板在各处独立实例化。
- C ABI 由平台标准统一规定且数十年稳定，是跨编译器乃至跨语言（Java JNI、Python ctypes）的唯一公共合约。
- 边界上还需掐死三类行为事故：C++ 异常穿越 `extern "C"` 边界为未定义行为（terminate 杀宿主）；内存所有权混乱（宿主与库分配器可能不同）；dlopen 期静态初始化顺序不可控。

设计文档中该接口原本未定义（§6.2 仅一句"C 风格函数集合"）。经 design-it-twice 三方案比较（过程与落选理由见 `docs/design/client-api-alternatives.md`）。

## Decision

对外导出 **6 函数 C 接口**（唯一对外头 `compress_client.h`，签名规格见设计文档 §6.2）：

`ccl_version` / `ccl_strerror` / `ccl_create` / `ccl_start` / `ccl_read` / `ccl_destroy`

配套纪律：

1. 边界纪律三条：异常不穿越（外壳 catch 转返回码）、内存所有权单向（宿主 buf 只进不出）、静态初始化收进 `ccl_create`；
2. 返回码 7 个、每个宿主可行动（EBUFSMALL 扩容重调 / EBUSY 背压 / EIO 记日志）；库内可自动消化的失败一律不上抛；
3. 唯一内部测试缝：`IRpcAdapter` 纯内部 C++ 接口（生产 BRPC / 测试 FakeRpc 注入），不经 C ABI 暴露；
4. 元信息文件头（磁盘契约）不做接口抽象；回写纪律（temp+fsync+rename）收在唯一文件 `atomic_replacer.cpp`。

## Rationale

- 形态对标 SQLite / leveldb 的 C 绑定，每个入口均有真实使用场景（启动自治、透明读、退出回收）背书；
- 否决 2 函数极简方案：其"库内拦截宿主 read"在 C 动态库中不可实现；
- 否决 18 函数灵活方案：以"缝需第二种真实实现"判据筛选，5 个候选缝中 2 个为假想（IScanPolicy/IWritebackPolicy 仅一种实现），接口税与回调重入风险大于收益；其内部 DI 与"磁盘格式钉死"纪律被定点吸收。

## Consequences

**正面**

- 宿主集成面小且稳定（6 函数 + 7 返回码），升级库不破坏宿主；
- 控制面逻辑经 FakeRpc 可离线单测；
- 排练生产形态：宿主独立团队维护的场景下 C ABI 是唯一可发布合约（本机 mock 宿主虽同编译环境，仍坚持此边界）。

**负面 / 代价**

- 接口一旦交付须保持 ABI 稳定，新增能力（如统计口 `ccl_stat`）只能走 v1.1 扩展或新函数；
- 宿主失去对库内部策略的直接干预能力（选点、退避均为库内策略）；
- C 外壳层增加少量转发代码与测试矩阵。

## 关联

- 探索证据：`docs/design/client-api-alternatives.md`
- 接口规格：设计文档 §6.2；类与文件映射：§7.2
- proto 语法选型另行决策（ADR-0004），不依赖本 ADR。
