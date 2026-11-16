// 统一编解码调用面：压缩级别 → 算法选择（浅压 LZ4 / 深压 LZMA）。
// 纯计算叶子：不碰 IO/网络。
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
 * 返回值即状态码：调用方先判返回值，成功后才可读取 *out_len。
 * 全部实现 noexcept，不抛异常。
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
   * @param[in]  out_cap  输出缓冲区容量（字节）。
   * @param[out] out_len  成功时为写入长度；返回 kBufSmall 时为所需容量。
   * @return kOk；kInvalidArg（参数非法）；kBufSmall（容量不足，*out_len 回报所需容量）。
   *
   * 写入契约：被调方写出恰好 *out_len 字节；[out_len, out_cap) 内容未指定，
   * 调用方不得读取；调用方预清零不要求。
   */
  virtual CompressErr Compress(const std::uint8_t* in, size_t in_len, std::uint8_t* out,
                               size_t out_cap, size_t* out_len) const noexcept = 0;

  /**
   * @brief 解压输入字节，还原原始数据。输入须为本接口 Compress 的输出。
   *
   * @param[in]  in       输入字节区首地址，不得为 nullptr。
   * @param[in]  in_len   输入长度（字节）。
   * @param[out] out      输出缓冲区首地址。
   * @param[in]  out_cap  输出缓冲区容量（字节）。
   * @param[out] out_len  成功时为写入长度；返回 kBufSmall 时为所需容量
   *                      （仅 LZ4 实现保证精确回报，LZMA 实现不保证）。
   * @return kOk；kInvalidArg（参数非法或数据损坏）；kBufSmall（容量不足）。
   *
   * 写入契约同 Compress：只保证前 *out_len 字节有效。
   */
  virtual CompressErr Decompress(const std::uint8_t* in, size_t in_len, std::uint8_t* out,
                                 size_t out_cap, size_t* out_len) const noexcept = 0;
};

class CodecFactory {
 public:
  // 级别 → 算法选择；未知级别返回 nullptr。
  static std::unique_ptr<ICodec> Create(CompressLevel level) noexcept;
};

}  // namespace compress
