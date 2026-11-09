// 统一编解码调用面：压缩级别 → 算法选择（浅压 LZ4 / 深压 LZMA）。
// 纯计算叶子：不碰 IO/网络。缓冲由调用方按 2× 原始大小纪律提供（设计文档 §3.1.3）。
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "compress/error_code.h"

namespace compress {

enum class CompressLevel : std::uint8_t {
    kShallow = 0,  // 浅压：LZ4，CPU 低，适合近期数据
    kDeep = 1,     // 深压：LZMA，CPU 高，适合长期未访问数据
};

/**
 * @brief 统一编解码接口。具体算法由 CodecFactory 按压缩级别装配。
 *
 * 成功时返回写入字节数并置 *err = kOk；失败返回 0，原因由 *err 指明。
 * 全部实现为 noexcept：错误经 err 返回，不抛异常。
 */
class ICodec {
public:
    virtual ~ICodec() = default;

    /**
     * @brief 压缩输入字节，输出压缩结果。
     *
     * @param[in]  in       输入字节区首地址，不得为 nullptr。
     * @param[in]  in_len   输入长度（字节），须大于 0。
     * @param[out] out      输出缓冲区首地址。
     * @param[in]  out_cap  输出缓冲区容量（字节）。为避免不可压数据越界，
     *                      调用方应提供不小于 2×in_len 的缓冲区。
     * @param[out] out_len  成功时为写入长度；返回 kBufSmall 时为所需容量。
     * @param[out] err      调用结果状态。
     * @return 成功返回写入字节数；失败返回 0。
     */
    virtual size_t Compress(const std::uint8_t* in, size_t in_len,
                            std::uint8_t* out, size_t out_cap, size_t* out_len,
                            CompressErr* err) const noexcept = 0;

    /**
     * @brief 解压输入字节，还原原始数据。输入须为本接口 Compress 的输出。
     *
     * @param[in]  in       输入字节区首地址，不得为 nullptr。
     * @param[in]  in_len   输入长度（字节）。
     * @param[out] out      输出缓冲区首地址。
     * @param[in]  out_cap  输出缓冲区容量（字节），应按原始数据大小提供。
     * @param[out] out_len  成功时为写入长度；返回 kBufSmall 时为所需容量
     *                      （LZ4 实现可精确回报；LZMA 实现不保证，
     *                      调用方不应依赖该值分配缓冲区）。
     * @param[out] err      调用结果状态。
     * @return 成功返回写入字节数；失败返回 0。
     */
    virtual size_t Decompress(const std::uint8_t* in, size_t in_len,
                              std::uint8_t* out, size_t out_cap, size_t* out_len,
                              CompressErr* err) const noexcept = 0;
};

class CodecFactory {
public:
    // 级别 → 算法选择；未知级别返回 nullptr。
    static std::unique_ptr<ICodec> Create(CompressLevel level) noexcept;
};

}  // namespace compress
