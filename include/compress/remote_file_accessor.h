// 块文件读访问器：读路径的全部文件 IO 收拢于此（pread 语义）。
// ClientCore::Read 命中压缩块时按文件头定位切片后取数；未压缩块直读透传（设计文档 §7.2）。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "compress/error_code.h"

namespace compress {

class RemoteFileAccessor {
public:
    RemoteFileAccessor() = default;

    /**
     * @brief 定位读块文件，语义对齐 pread：不隐含偏移推进，返回实际可读字节数。
     * @param path 块文件路径
     * @param offset 读取起始偏移
     * @param buf 调用方输出缓冲
     * @param cap buf 容量（字节）
     * @param out_len 带回实际写入字节数；文件末尾截断时带回实际可读量
     */
    CompressErr Pread(const std::string& path, std::uint64_t offset, char* buf,
                      std::size_t cap, std::size_t* out_len) noexcept;

    CompressErr FileSize(const std::string& path, std::uint64_t* out_size) noexcept;
};

}  // namespace compress
