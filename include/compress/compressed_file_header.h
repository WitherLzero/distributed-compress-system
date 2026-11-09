// 块级自包含文件头（issue 02-D 定稿布局）：
// 定长头 128B + 每切片 8B 条目；偏移量不存，由顺序与定长推导。
// 头 + 条目合计预算 ≤ 4KB（一页），读路径的元信息读取保持单次 IO（设计文档 §7.1）。
// 物理含义：块文件开头 4 字节等于魔数 = 已压缩，无头 = 未压缩。
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "compress/error_code.h"

namespace compress {

constexpr std::uint32_t kFormatVersion = 1;
constexpr size_t kFixedHeaderSize = 128;
constexpr size_t kSliceEntrySize = 8;
constexpr size_t kHeaderBudgetBytes = 4096;             // 一页预算：头 + 条目合计
constexpr size_t kDefaultSliceSize = 4 * 1024 * 1024;   // 切片定长 4MB（02-D 钉死）

#pragma pack(push, 1)
struct FileHeader {
    char          magic[4];          // "CPS1"
    std::uint16_t format_version;
    std::uint8_t  algorithm;         // 0 = LZ4 浅压，1 = LZMA 深压（CompressLevel）
    std::uint8_t  flags;
    std::uint32_t slice_count;
    std::uint64_t origin_block_size;
    std::uint8_t  reserved[108];
};

struct SliceEntry {
    std::uint32_t compressed_len;
    std::uint32_t crc32;             // 切片原始字节的 CRC32
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == kFixedHeaderSize, "FileHeader must stay 128 bytes");
static_assert(sizeof(SliceEntry) == kSliceEntrySize, "SliceEntry must stay 8 bytes");

// 序列化后的头区总字节数：定长头 + 全部条目（128 + slice_count × 8）。
size_t SerializedHeaderSize(std::uint32_t slice_count) noexcept;

/**
 * @brief 将定长头与条目序列序列化为连续字节区。仅做前提检查，不校验字段语义。
 *
 * @param[in]  header      待序列化的定长头，字段须已由调用方完整填写。
 * @param[in]  entries     条目数组；slice_count 为 0 时可为 nullptr。
 * @param[in]  slice_count 切片数。头与条目合计超过容量预算时返回 kInvalidArg。
 * @param[out] out         输出字节区，容量不得小于 SerializedHeaderSize(slice_count)。
 * @param[in]  cap         out 的容量（字节）。
 * @param[out] out_len     成功时为写入字节数；返回 kBufSmall 时为所需容量。
 * @return kOk 表示成功；kBufSmall 表示容量不足；kInvalidArg 表示参数非法
 *         或合计超出容量预算。
 */
CompressErr SerializeHeader(const FileHeader& header, const SliceEntry* entries,
                            std::uint32_t slice_count,
                            std::uint8_t* out, size_t cap, size_t* out_len) noexcept;

/**
 * @brief 从字节区解析定长头与条目序列，并执行完整性校验。
 *
 * 校验顺序：字节区长度 → 魔数 → 格式版本 → 声明切片数的容量预算 → 条目完整性。
 * 任一环节不通过即返回，不产生部分填充的输出。
 *
 * @param[in]  in      字节区首地址，不得为 nullptr。
 * @param[in]  len     字节区长度（字节）。
 * @param[out] out     解析得到的定长头。
 * @param[out] entries 解析得到的条目序列，按实际切片数 resize。
 * @return kOk 表示成功；kBufSmall 表示长度不足；kInvalidArg 表示魔数不符、
 *         格式版本不符或声明的切片数超出容量预算。
 */
CompressErr ParseHeader(const std::uint8_t* in, size_t len,
                        FileHeader* out, std::vector<SliceEntry>* entries) noexcept;

// 读路径快速判定：前 4 字节是否为魔数（「无头即未压缩」的物理含义）。
bool LooksCompressed(const std::uint8_t* prefix4) noexcept;

// 切片 i 的原始数据偏移 = i × slice_size（定长推导，不存盘）。
std::uint64_t OriginOffset(std::uint32_t slice_index, size_t slice_size) noexcept;

// 数据区起点偏移 = SerializedHeaderSize(slice_count)。
std::uint64_t DataRegionOffset(std::uint32_t slice_count) noexcept;

// 切片 i 的压缩数据偏移 = 数据区起点 + 前序条目 compressed_len 的前缀和。
std::uint64_t CompressedOffset(const std::vector<SliceEntry>& entries,
                               std::uint32_t slice_index) noexcept;

}  // namespace compress
