// client 侧组合根：创建（Assemble）装配全部组件，Start/Shutdown 管线程生命周期（设计文档 §7.2）。
// 公开接口遵循 ADR-0005 四纪律：无异常出口（noexcept 边界）、无全局配置注册（Options 值注入）、
// Read 语义对齐 pread、零静态初始化。
#pragma once

#include <cstddef>
#include <cstdint>

#include "compress/error_code.h"
#include "compress/options.h"

namespace compress {

class ClientCore {
public:
    ClientCore() = default;

    CompressErr Assemble(const Options& opts) noexcept;  // 装配以下组件并建立依赖连线
    CompressErr Start() noexcept;               // 起扫描/压缩/列表同步线程
    void Shutdown() noexcept;                   // 排空在途任务、join 线程、释放组件

    /**
     * @brief Read 门面实现，语义对齐 pread：不隐含偏移推进，返回实际可读字节数。
     * 命中压缩块：读文件头 → 定位切片 → 拉取解压；未压缩块（无头）：直读透传。
     * @param path 块文件路径
     * @param offset 读取起始偏移（原始未压缩坐标系）
     * @param buf 调用方输出缓冲
     * @param cap buf 容量（字节）
     * @param out_len 带回实际写入字节数；buf 不足时返 kBufSmall 并带回所需容量
     */
    CompressErr Read(const char* path, std::uint64_t offset, char* buf, std::size_t cap,
                     std::size_t* out_len) noexcept;
};

}  // namespace compress
