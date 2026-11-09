// 统一编解码封装实现：LZ4 浅压 / LZMA 深压，同一调用面（codec.h）。
// 算法失败（数据无法解析）归入 kInvalidArg：对外语义见设计文档 §6.2.2，
// §5.5 业务层失败为库内日志原因，不经此层上抛。
#include "compress/codec.h"

#include <lz4frame.h>
#include <lzma.h>

namespace compress {
namespace {

// LZ4 帧格式（one-shot）：写 contentSize 便于 kBufSmall 时报所需容量，
// 开内容校验和，解压侧能检出数据损坏。
class Lz4Codec final : public ICodec {
public:
    size_t Compress(const std::uint8_t* in, size_t in_len,
                    std::uint8_t* out, size_t out_cap, size_t* out_len,
                    CompressErr* err) const noexcept override {
        if (!err) return 0;
        if (!in || !out || !out_len || in_len == 0) { *err = CompressErr::kInvalidArg; return 0; }

        LZ4F_preferences_t prefs{};
        prefs.frameInfo.contentSize = in_len;
        prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;

        const size_t bound = LZ4F_compressFrameBound(in_len, &prefs);
        if (out_cap < bound) { *out_len = bound; *err = CompressErr::kBufSmall; return 0; }

        const size_t n = LZ4F_compressFrame(out, out_cap, in, in_len, &prefs);
        if (LZ4F_isError(n)) { *err = CompressErr::kInvalidArg; return 0; }
        *out_len = n;
        *err = CompressErr::kOk;
        return n;
    }

    size_t Decompress(const std::uint8_t* in, size_t in_len,
                      std::uint8_t* out, size_t out_cap, size_t* out_len,
                      CompressErr* err) const noexcept override {
        if (!err) return 0;
        if (!in || !out || !out_len || in_len == 0) { *err = CompressErr::kInvalidArg; return 0; }

        LZ4F_dctx* dctx = nullptr;
        if (LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION))) {
            *err = CompressErr::kInvalidArg;
            return 0;
        }

        size_t dst_pos = 0;
        size_t src_pos = 0;
        CompressErr e = CompressErr::kOk;
        while (src_pos < in_len) {
            size_t dst_size = out_cap - dst_pos;
            size_t src_size = in_len - src_pos;
            const size_t hint = LZ4F_decompress(dctx, out + dst_pos, &dst_size,
                                                in + src_pos, &src_size, nullptr);
            dst_pos += dst_size;
            src_pos += src_size;
            if (LZ4F_isError(hint)) { e = CompressErr::kInvalidArg; break; }
            if (dst_size == 0 && src_size == 0) { e = CompressErr::kBufSmall; break; }
            if (hint == 0) break;  // 帧已完整消费
        }
        if (e == CompressErr::kBufSmall) {
            LZ4F_frameInfo_t fi{};
            size_t hdr_len = in_len;
            if (!LZ4F_isError(LZ4F_getFrameInfo(dctx, &fi, in, &hdr_len)) &&
                fi.contentSize != 0) {
                *out_len = static_cast<size_t>(fi.contentSize);
            }
        }
        LZ4F_freeDecompressionContext(dctx);
        if (e == CompressErr::kOk) { *out_len = dst_pos; }
        *err = e;
        return e == CompressErr::kOk ? dst_pos : 0;
    }
};

// LZMA one-shot（liblzma easy API），CRC64 校验。
class LzmaCodec final : public ICodec {
public:
    size_t Compress(const std::uint8_t* in, size_t in_len,
                    std::uint8_t* out, size_t out_cap, size_t* out_len,
                    CompressErr* err) const noexcept override {
        if (!err) return 0;
        if (!in || !out || !out_len || in_len == 0) { *err = CompressErr::kInvalidArg; return 0; }

        const size_t bound = lzma_stream_buffer_bound(in_len);
        if (out_cap < bound) { *out_len = bound; *err = CompressErr::kBufSmall; return 0; }

        size_t out_pos = 0;
        const lzma_ret r = lzma_easy_buffer_encode(LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64,
                                                   nullptr, in, in_len, out, &out_pos, out_cap);
        if (r != LZMA_OK) { *err = CompressErr::kInvalidArg; return 0; }
        *out_len = out_pos;
        *err = CompressErr::kOk;
        return out_pos;
    }

    size_t Decompress(const std::uint8_t* in, size_t in_len,
                      std::uint8_t* out, size_t out_cap, size_t* out_len,
                      CompressErr* err) const noexcept override {
        if (!err) return 0;
        if (!in || !out || !out_len || in_len == 0) { *err = CompressErr::kInvalidArg; return 0; }

        std::uint64_t memlimit = ~static_cast<std::uint64_t>(0);
        size_t in_pos = 0;
        size_t out_pos = 0;
        const lzma_ret r = lzma_stream_buffer_decode(&memlimit, 0, nullptr,
                                                     in, &in_pos, in_len,
                                                     out, &out_pos, out_cap);
        if (r == LZMA_OK) {
            *out_len = out_pos;
            *err = CompressErr::kOk;
            return out_pos;
        }
        if (r == LZMA_BUF_ERROR) {
            // 输入没消费完 = 数据截断/损坏；消费完仍装不下 = 缓冲不足。
            *err = (in_pos < in_len) ? CompressErr::kInvalidArg : CompressErr::kBufSmall;
            return 0;
        }
        *err = CompressErr::kInvalidArg;
        return 0;
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
