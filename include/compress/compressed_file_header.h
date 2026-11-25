// 块级自包含文件头（ADR-0006 定稿布局）：
// 定长头 128B + 每切片 16B 条目；条目直接存压缩后偏移 compressed_offset，
// 随机读第 N 片 O(1) 直接定位，不做前缀和累加（累加是 O(N)，违反随机读原则）。
// 头 + 条目合计预算 ≤ 4KB（一页），读路径的元信息读取保持单次 IO（设计文档 §7.1）。
// 物理含义：块文件开头 4 字节等于魔数 = 已压缩，无头 = 未压缩。
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>  // offsetof 断言见结构体定义下方纪律注释

#include "compress/error_code.h"

namespace compress {

constexpr std::uint32_t kFormatVersion = 1;
constexpr size_t kFixedHeaderSize = 128;
constexpr size_t kSliceEntrySize = 16;
constexpr size_t kHeaderBudgetBytes = 4096;            // 一页预算：头 + 条目合计
constexpr size_t kDefaultSliceSize = 4 * 1024 * 1024;  // 切片定长 4MB（钉死）

#pragma pack(push, 1)
struct FileHeader {
  char magic[4];  // "CPS1"
  std::uint16_t format_version;
  std::uint8_t algorithm;  // 0 = LZ4 浅压，1 = LZMA 深压（CompressLevel）
  std::uint8_t flags;
  std::uint32_t slice_count;
  std::uint64_t origin_block_size;
  std::uint64_t compressed_len;  // 整块压缩数据总长（数据区全部条目 len 之和）
  std::uint8_t reserved[100];
};

struct SliceEntry {
  std::uint64_t compressed_offset;  // 压后数据在文件中的绝对偏移：随机读直接定位
  std::uint32_t compressed_len;
  std::uint32_t crc32;  // 切片原始字节的 CRC32
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == kFixedHeaderSize, "FileHeader must stay 128 bytes");
static_assert(sizeof(SliceEntry) == kSliceEntrySize, "SliceEntry must stay 16 bytes");

// 磁盘布局稳定性纪律（ADR-0006）：SliceEntry 全字段为语义字节，序列化走整块 memcpy，
// 无 reserved 可兜底——任何字段增删或布局漂移都必须先修订 ADR，再改这里。
// offsetof 断言逐字段钉死偏移：pack(1) 被移除、字段顺序调整或引入对齐洞，编译期即失败，
// 垃圾字节没有进盘通道。FileHeader 的 reserved 序列化时被值初始化消毒，不依赖此处。
static_assert(offsetof(SliceEntry, compressed_offset) == 0, "SliceEntry disk offset drift");
static_assert(offsetof(SliceEntry, compressed_len) == 8, "SliceEntry disk offset drift");
static_assert(offsetof(SliceEntry, crc32) == 12, "SliceEntry disk offset drift");
static_assert(offsetof(FileHeader, slice_count) == 8, "FileHeader disk offset drift");
static_assert(offsetof(FileHeader, origin_block_size) == 12, "FileHeader disk offset drift");
static_assert(offsetof(FileHeader, compressed_len) == 20, "FileHeader disk offset drift");

// 布局自检恒等式（写路径须保证、读路径可校验）：
//   entries[i].compressed_offset + entries[i].compressed_len == entries[i+1].compressed_offset
//   entries[0].compressed_offset == DataRegionOffset(slice_count) == 128 + 16 × slice_count
// 切片 i 的原始数据偏移 = i × slice_size（定长推导，不存盘）。

// 序列化后的头区总字节数：定长头 + 全部条目（128 + slice_count × 16）。
size_t SerializedHeaderSize(std::uint32_t slice_count) noexcept;

/**
 * @brief 将定长头与条目序列序列化为连续字节区。仅做前提检查，不校验字段语义。
 *
 * 确定性契约：输出只含头与条目的语义字段；调用方传入结构体的 reserved 与
 * 对齐洞不进入输出——同输入两次序列化逐字节相同。
 *
 * @param[in]  header      待序列化的定长头，语义字段须已由调用方完整填写。
 * @param[in]  entries     条目数组；slice_count 为 0 时可为 nullptr。
 * @param[in]  slice_count 切片数。头与条目合计超过容量预算时返回 kInvalidArg。
 * @param[out] out         输出字节区，容量不得小于 SerializedHeaderSize(slice_count)。
 * @param[in]  cap         out 的容量（字节）。
 * @param[out] out_len     成功时为写入字节数；返回 kBufSmall 时为所需容量。
 * @return kOk 表示成功；kBufSmall 表示容量不足；kInvalidArg 表示参数非法
 *         或合计超出容量预算。
 *
 * 写入契约（照 POSIX read 措辞）：被调方负责写出恰好 *out_len 字节；
 * [out_len, cap) 区间内容未指定，调用方不得读取。
 */
CompressErr SerializeHeader(const FileHeader& header, const SliceEntry* entries,
                            std::uint32_t slice_count, std::uint8_t* out, size_t cap,
                            size_t* out_len) noexcept;

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
CompressErr ParseHeader(const std::uint8_t* in, size_t len, FileHeader* out,
                        std::vector<SliceEntry>* entries) noexcept;

// 读路径快速判定：前 4 字节是否为魔数（「无头即未压缩」的物理含义）。
bool LooksCompressed(const std::uint8_t* prefix4) noexcept;

// 切片 i 的原始数据偏移 = i × slice_size（定长推导，不存盘）。
std::uint64_t OriginOffset(std::uint32_t slice_index, size_t slice_size) noexcept;

// 数据区起点偏移 = SerializedHeaderSize(slice_count)。
std::uint64_t DataRegionOffset(std::uint32_t slice_count) noexcept;

// 布局一致性校验：按恒等式逐条检查条目链（offset+len == 下一 offset），
// 并核对 S0.offset == 数据区起点。通过返回 kOk，否则 kInvalidArg。
CompressErr ValidateEntryChain(const std::vector<SliceEntry>& entries) noexcept;

}  // namespace compress
