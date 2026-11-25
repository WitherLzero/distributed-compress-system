// 统一编解码封装实现：LZ4 浅压 / LZMA 深压，同一调用面（codec.h）。
// 算法失败（数据无法解析）归入 kInvalidArg：对外语义见设计文档 §6.2.2，
// §5.5 业务层失败为库内日志原因，不经此层上抛。
//
// 实现备注（契约的依据，勿上翻进头文件）：
// - out_cap 必要性：裸指针不带长度，底层 dstCapacity 与 kBufSmall 的精确回报
//   都以它为输入（业界 LZ4F/zstd/lzma/brotli/POSIX read 无一例外带输出容量）；
//   压缩侧调用方按 2× 原始大小纪律提供缓冲（设计文档 §3.1.3）。
// - 解压侧 out_cap 兜两个底：正常路径定长切片输出上界 min(4MB, 块尾余量)
//   精确已知；坏帧产出长度异常时靠它转 kInvalidArg 而非越界写；读路径任意
//   窗口（宿主缓冲 < 4MB）也依赖它。
// - LZ4 帧 contentSize（压缩侧写入）使解压 kBufSmall 时能精确回报所需容量；
//   LZMA 的原始大小在流尾索引，one-shot API 不暴露，不保证回报。
// - 参数类型现状 uint8_t*（lzma 同款）；void*/char* 选型待定，见 issue 03 第 2 项。
#include "compress/codec.h"

#include <lz4frame.h>
#include <lzma.h>

namespace compress {
namespace {

// LZ4 帧格式（one-shot）：写 contentSize 便于 kBufSmall 时报所需容量，
// 开内容校验和，解压侧能检出数据损坏。
class Lz4Codec final : public ICodec {
 public:
  CompressErr Compress(const std::uint8_t* in, size_t in_len, std::uint8_t* out, size_t out_cap,
                       size_t* out_len) const noexcept override {
    if (!in || !out || !out_len || in_len == 0) {
      return CompressErr::kInvalidArg;
    }

    LZ4F_preferences_t prefs{};
    prefs.frameInfo.contentSize = in_len;
    prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;

    const size_t bound = LZ4F_compressFrameBound(in_len, &prefs);
    if (out_cap < bound) {
      *out_len = bound;
      return CompressErr::kBufSmall;
    }

    const size_t n = LZ4F_compressFrame(out, out_cap, in, in_len, &prefs);
    if (LZ4F_isError(n)) {
      return CompressErr::kInvalidArg;
    }
    *out_len = n;
    return CompressErr::kOk;
  }

  CompressErr Decompress(const std::uint8_t* in, size_t in_len, std::uint8_t* out, size_t out_cap,
                         size_t* out_len) const noexcept override {
    if (!in || !out || !out_len || in_len == 0) {
      return CompressErr::kInvalidArg;
    }

    LZ4F_dctx* dctx = nullptr;
    if (LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION))) {
      return CompressErr::kInvalidArg;
    }

    size_t dst_pos = 0;
    size_t src_pos = 0;
    CompressErr e = CompressErr::kOk;
    while (src_pos < in_len) {
      size_t dst_size = out_cap - dst_pos;
      size_t src_size = in_len - src_pos;
      const size_t hint =
          LZ4F_decompress(dctx, out + dst_pos, &dst_size, in + src_pos, &src_size, nullptr);
      dst_pos += dst_size;
      src_pos += src_size;
      if (LZ4F_isError(hint)) {
        e = CompressErr::kInvalidArg;
        break;
      }
      if (dst_size == 0 && src_size == 0) {
        e = CompressErr::kBufSmall;
        break;
      }
      if (hint == 0) break;  // 帧已完整消费
    }
    if (e == CompressErr::kBufSmall) {
      LZ4F_frameInfo_t fi{};
      size_t hdr_len = in_len;
      if (!LZ4F_isError(LZ4F_getFrameInfo(dctx, &fi, in, &hdr_len)) && fi.contentSize != 0) {
        *out_len = static_cast<size_t>(fi.contentSize);
      }
    }
    LZ4F_freeDecompressionContext(dctx);
    if (e == CompressErr::kOk) {
      *out_len = dst_pos;
    }
    return e;
  }
};

// LZMA one-shot（liblzma easy API），CRC64 校验。
class LzmaCodec final : public ICodec {
 public:
  CompressErr Compress(const std::uint8_t* in, size_t in_len, std::uint8_t* out, size_t out_cap,
                       size_t* out_len) const noexcept override {
    if (!in || !out || !out_len || in_len == 0) {
      return CompressErr::kInvalidArg;
    }

    const size_t bound = lzma_stream_buffer_bound(in_len);
    if (out_cap < bound) {
      *out_len = bound;
      return CompressErr::kBufSmall;
    }

    size_t out_pos = 0;
    const lzma_ret r = lzma_easy_buffer_encode(LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64, nullptr, in,
                                               in_len, out, &out_pos, out_cap);
    if (r != LZMA_OK) {
      return CompressErr::kInvalidArg;
    }
    *out_len = out_pos;
    return CompressErr::kOk;
  }

  CompressErr Decompress(const std::uint8_t* in, size_t in_len, std::uint8_t* out, size_t out_cap,
                         size_t* out_len) const noexcept override {
    if (!in || !out || !out_len || in_len == 0) {
      return CompressErr::kInvalidArg;
    }

    std::uint64_t memlimit = ~static_cast<std::uint64_t>(0);
    size_t in_pos = 0;
    size_t out_pos = 0;
    const lzma_ret r = lzma_stream_buffer_decode(&memlimit, 0, nullptr, in, &in_pos, in_len, out,
                                                 &out_pos, out_cap);
    if (r == LZMA_OK) {
      *out_len = out_pos;
      return CompressErr::kOk;
    }
    if (r == LZMA_BUF_ERROR) {
      // 输入没消费完 = 数据截断/损坏；消费完仍装不下 = 缓冲不足。
      return (in_pos < in_len) ? CompressErr::kInvalidArg : CompressErr::kBufSmall;
    }
    return CompressErr::kInvalidArg;
  }
};

}  // namespace

std::unique_ptr<ICodec> CodecFactory::Create(CompressLevel level) noexcept {
  switch (level) {
    case CompressLevel::kShallow:
      return std::make_unique<Lz4Codec>();
    case CompressLevel::kDeep:
      return std::make_unique<LzmaCodec>();
  }
  return nullptr;
}

}  // namespace compress
