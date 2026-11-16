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
                            std::uint32_t slice_count, std::uint8_t* out, size_t cap,
                            size_t* out_len) noexcept {
  if (!out || !out_len) return CompressErr::kInvalidArg;
  if (slice_count > 0 && !entries) return CompressErr::kInvalidArg;

  const size_t need = SerializedHeaderSize(slice_count);
  if (need > kHeaderBudgetBytes) return CompressErr::kInvalidArg;  // 超一页预算
  if (cap < need) {
    *out_len = need;
    return CompressErr::kBufSmall;
  }

  // 确定性（双保险）：
  // 1) out 先清零写入区——被 copy 之前 memset 过（避免残留旧数据进输出）；
  // 2) 值初始化本地副本只搬语义字段——调用方结构体的 reserved 与对齐洞里的
  //    malloc 垃圾不得进入输出（同输入两次序列化逐字节相同）。
  std::memset(out, 0, need);
  FileHeader clean{};
  std::memcpy(clean.magic, header.magic, sizeof(clean.magic));
  clean.format_version = header.format_version;
  clean.algorithm = header.algorithm;
  clean.flags = header.flags;
  clean.slice_count = header.slice_count;
  clean.origin_block_size = header.origin_block_size;
  clean.compressed_len = header.compressed_len;

  std::memcpy(out, &clean, kFixedHeaderSize);
  if (slice_count > 0) {
    std::memcpy(out + kFixedHeaderSize, entries,
                static_cast<size_t>(slice_count) * kSliceEntrySize);
  }
  *out_len = need;
  return CompressErr::kOk;
}

CompressErr ParseHeader(const std::uint8_t* in, size_t len, FileHeader* out,
                        std::vector<SliceEntry>* entries) noexcept {
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

CompressErr ValidateEntryChain(const std::vector<SliceEntry>& entries) noexcept {
  const std::uint64_t expect_first = DataRegionOffset(static_cast<std::uint32_t>(entries.size()));
  if (!entries.empty() && entries.front().compressed_offset != expect_first) {
    return CompressErr::kInvalidArg;
  }
  for (size_t i = 0; i + 1 < entries.size(); ++i) {
    const std::uint64_t next_expect =
        entries[i].compressed_offset + entries[i].compressed_len;
    if (next_expect != entries[i + 1].compressed_offset) {
      return CompressErr::kInvalidArg;
    }
  }
  return CompressErr::kOk;
}

}  // namespace compress
