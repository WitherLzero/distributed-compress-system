// 块级文件头读写实现（compressed_file_header.h）。
#include "compress/compressed_file_header.h"

#include <cstring>

namespace compress {
namespace {

constexpr char kMagic[4] = {'C', 'P', 'S', '1'};

bool MagicEquals(const char* magic) noexcept {
    return std::memcmp(magic, kMagic, 4) == 0;
}

}  // namespace

size_t SerializedHeaderSize(std::uint32_t slice_count) noexcept {
    return kFixedHeaderSize + static_cast<size_t>(slice_count) * kSliceEntrySize;
}

CompressErr SerializeHeader(const FileHeader& header, const SliceEntry* entries,
                            std::uint32_t slice_count,
                            std::uint8_t* out, size_t cap, size_t* out_len) noexcept {
    if (!out || !out_len) return CompressErr::kInvalidArg;
    if (slice_count > 0 && !entries) return CompressErr::kInvalidArg;

    const size_t need = SerializedHeaderSize(slice_count);
    if (need > kHeaderBudgetBytes) return CompressErr::kInvalidArg;  // 超一页预算
    if (cap < need) { *out_len = need; return CompressErr::kBufSmall; }

    std::memcpy(out, &header, kFixedHeaderSize);
    if (slice_count > 0) {
        std::memcpy(out + kFixedHeaderSize, entries,
                    static_cast<size_t>(slice_count) * kSliceEntrySize);
    }
    *out_len = need;
    return CompressErr::kOk;
}

CompressErr ParseHeader(const std::uint8_t* in, size_t len,
                        FileHeader* out, std::vector<SliceEntry>* entries) noexcept {
    if (!in || !out || !entries) return CompressErr::kInvalidArg;
    if (len < kFixedHeaderSize) return CompressErr::kBufSmall;  // 连定长头都不够

    FileHeader header{};
    std::memcpy(&header, in, kFixedHeaderSize);
    if (!MagicEquals(header.magic)) return CompressErr::kInvalidArg;
    if (header.format_version != kFormatVersion) return CompressErr::kInvalidArg;

    const size_t need = SerializedHeaderSize(header.slice_count);
    if (need > kHeaderBudgetBytes) return CompressErr::kInvalidArg;
    if (len < need) return CompressErr::kBufSmall;

    entries->resize(header.slice_count);
    if (header.slice_count > 0) {
        std::memcpy(entries->data(), in + kFixedHeaderSize,
                    static_cast<size_t>(header.slice_count) * kSliceEntrySize);
    }
    *out = header;
    return CompressErr::kOk;
}

bool LooksCompressed(const std::uint8_t* prefix4) noexcept {
    return prefix4 != nullptr && std::memcmp(prefix4, kMagic, 4) == 0;
}

std::uint64_t OriginOffset(std::uint32_t slice_index, size_t slice_size) noexcept {
    return static_cast<std::uint64_t>(slice_index) * slice_size;
}

std::uint64_t DataRegionOffset(std::uint32_t slice_count) noexcept {
    return SerializedHeaderSize(slice_count);
}

std::uint64_t CompressedOffset(const std::vector<SliceEntry>& entries,
                               std::uint32_t slice_index) noexcept {
    std::uint64_t off = DataRegionOffset(static_cast<std::uint32_t>(entries.size()));
    for (std::uint32_t i = 0; i < slice_index && i < entries.size(); ++i) {
        off += entries[i].compressed_len;
    }
    return off;
}

}  // namespace compress
