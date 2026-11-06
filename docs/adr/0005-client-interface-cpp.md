# ADR-0005: compress_client 对外接口回退为 C++ 接口

- **Status**: Accepted（2026-11-03 第 3 课导师反馈确认方向；**2026-11-07 接口形态经 issue 02-A 讨论定稿**，见「已定接口形态」节）
- **Date**: 2026-11-03
- **Deciders**: 导师（第 3 课会议）+ 林昕祺（学员）

## Context

[ADR-0003](0003-c-abi-client-interface.md) 决定 `compress_client` 对宿主暴露 C ABI 最小函数集（`ccl_*` 6 函数），理由之一是"C++ 无稳定 ABI，宿主与库为独立编译的二进制"。

第 3 课（2026-11-03）导师反馈重新审视了这一前提：

- **宿主假设是类 HDFS 的 C++ 系统**：宿主进程本身就是 C++ 编写，与 `compress_client` 同语言，不存在跨编译器/跨语言的 ABI 不稳定问题——C ABI 解决的是一个本项目不存在的问题；
- **C ABI 增加复杂度且易出错**：C 接口需要手工管理内存所有权、异常边界转换、不透明句柄等，对实习项目的实现与调试都是额外负担；
- **跨语言（Java JNI）才需要 C ABI**：若宿主为 Java 等异构语言，JNI 调用 C 接口在工程上极易 crash，除非确有跨语言需求，不应预先支付 C ABI 的成本。

## Decision

`compress_client` 对外接口由 C ABI 回退为 **C++ 接口**。具体形态（函数式 vs 类、错误码 vs 异常、头文件组织）经讨论定稿后更新设计文档 §6.2 / §7.2（见 issue 02-A）。

本 ADR **取代 ADR-0003**。

## 已定接口形态（2026-11-07，issue 02-A 定稿）

对外接口收敛为「一个类 + 一个配置结构体 + 一个错误码枚举」（`include/compress_client/client.h`，宿主唯一可见头文件，Pimpl 隐藏内部类型）：

```cpp
namespace compress::client {

enum class CompressErr { kOk = 0, kInvalidArg, kWrongState, kIo,
                         kBufSmall, kBusy, /* …§5.5 体系… */ };

struct Options {                  // 全部可配置项；数值默认值占位、联调标定（§5.6）
    std::string data_dir;
    std::string master_addr = "127.0.0.1:8001";
    int scan_interval_sec = 30;
    /* …压缩率阈值等数值待 issue 02-B 定稿… */
};

class Client {
public:
    static Result Create(const Options& opts, std::unique_ptr<Client>* out); // 只装配，不起线程
    CompressErr Start();              // 幂等
    CompressErr Read(const std::string& path, uint64_t offset,
                     void* buf, size_t cap, size_t* out_len);  // 同步、可多线程并发
    void Shutdown() noexcept;         // 幂等
    ~Client();                        // 未 Shutdown 则自动兜底（RAII）
};

const char* Version() noexcept;       // 宿主启动时校验兼容性
}
```

四条接口纪律：

1. **接口 `noexcept`**：库内异常在边界统一 catch 转 `CompressErr`。ADR-0003 时代「异常不得穿越边界」的纪律保留，理由从「C ABI 约束」改为「一个库内 bug 不应 terminate 宿主进程」；
2. **库内禁止 `DEFINE_*`（gflags）**：gflags 注册表是进程级全局且扁平无命名空间，库在 dlopen 时注册与宿主冲突即 abort；且库 flag 的值只能经宿主命令行传入，构成对宿主启动契约的隐式耦合。gflags 仅用于带 `main()` 的二进制（master / server / mock_datanode），库配置一律经 `Options` 注入；
3. **`Read` 对齐 POSIX `pread`**：调用方出 buffer、同步阻塞、多线程并发；缓冲不足时 `*out_len` 带回所需容量返回 `kBufSmall`（继承 C ABI 版 `CCL_EBUFSMALL` 语义）；
4. **零 dlopen 期静态初始化**：全部装配收进显式 `Create`（继承自 ADR-0003 纪律）。

## Rationale

- 同语言集成的动态库应使用自然的 C++ 接口（类 + 异常/错误码），避免 C 外壳的转发损耗与出错面；
- 6 周实习项目的工程复杂度应集中在分布式压缩核心，而非边界胶水层；
- 若未来出现跨语言宿主需求，可在 C++ 接口之上再包一层 C ABI 导出（增量扩展，不回退本决策）。

## Consequences

**正面**

- 接口形态自然，宿主集成代码量减少，无 `extern "C"` 边界纪律负担；
- 可直接使用 C++ 类型（`std::string`、`std::span` 等）传递元信息，减少手工序列化。

**负面 / 代价**

- 失去"跨编译器/跨语言即插即用"的属性——若宿主为异构语言，需要额外包装层；
- 接口变更需要与宿主同步编译（同语言下为常规操作）。

## 关联

- 取代：[ADR-0003](0003-c-abi-client-interface.md)（Superseded）
- 接口规格更新位置：ADR 本节为定稿记录；设计文档 §6.2 重写随 issue 02 B/C/D 全部定稿后一次落
- 讨论记录：`meeting/见识星球 昕棋 软件研发实习第3课.md`（01:05:31–01:07:39）；issue 02-A 讨论（2026-11-07）
