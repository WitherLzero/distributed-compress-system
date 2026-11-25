// 块级文件头读写单测。布局按 ADR-0006 定稿：128B 定长头 + 16B 条目（含 compressed_offset），
// 合计 ≤ 4KB；序列化确定性：reserved/对齐垃圾不进输出。
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "compress/compressed_file_header.h"

namespace {

compress::FileHeader MakeHeader(std::uint32_t slice_count, std::uint64_t origin_size,
                                std::uint64_t compressed_len = 0) {
  compress::FileHeader h{};
  std::memcpy(h.magic, "CPS1", 4);
  h.format_version = compress::kFormatVersion;
  h.algorithm = 1;  // LZMA 深压
  h.flags = 0;
  h.slice_count = slice_count;
  h.origin_block_size = origin_size;
  h.compressed_len = compressed_len;
  return h;
}

// 按布局恒等式构造条目链：S0.offset = 128 + 16N，之后 offset 顺次累加。
std::vector<compress::SliceEntry> MakeChain(std::uint32_t slice_count,
                                            const std::uint32_t lens[]) {
  std::vector<compress::SliceEntry> entries(slice_count);
  std::uint64_t off = compress::DataRegionOffset(slice_count);
  for (std::uint32_t i = 0; i < slice_count; ++i) {
    entries[i].compressed_offset = off;
    entries[i].compressed_len = lens[i];
    entries[i].crc32 = 0x01020304u + i;
    off += lens[i];
  }
  return entries;
}

}  // namespace

TEST(HeaderTest, SerializedSizeIs128Plus16PerSlice) {
  EXPECT_EQ(compress::SerializedHeaderSize(0), 128u);
  EXPECT_EQ(compress::SerializedHeaderSize(3), 128u + 3 * 16u);
  EXPECT_EQ(compress::SerializedHeaderSize(16), 128u + 16 * 16u);  // 64MB/4MB 整块 = 384B
}

TEST(HeaderTest, WriteReadRoundTrip) {
  constexpr std::uint32_t kSlices = 3;
  const compress::FileHeader want = MakeHeader(kSlices, 10u * 1024 * 1024, 2800 * 1024);
  std::vector<compress::SliceEntry> entries(kSlices);
  entries[0] = {128u + 3 * 16u, 900 * 1024, 0xAABBCCDD};
  entries[1] = {128u + 3 * 16u + 900u * 1024, 1100 * 1024, 0x11223344};
  entries[2] = {128u + 3 * 16u + 2000u * 1024, 800 * 1024, 0x55667788};

  std::vector<std::uint8_t> buf(compress::SerializedHeaderSize(kSlices));
  size_t len = 0;
  ASSERT_EQ(compress::SerializeHeader(want, entries.data(), kSlices, buf.data(), buf.size(), &len),
            compress::CompressErr::kOk);
  ASSERT_EQ(len, buf.size());

  compress::FileHeader got{};
  std::vector<compress::SliceEntry> got_entries;
  ASSERT_EQ(compress::ParseHeader(buf.data(), buf.size(), &got, &got_entries),
            compress::CompressErr::kOk);

  EXPECT_EQ(std::memcmp(got.magic, "CPS1", 4), 0);
  EXPECT_EQ(got.format_version, want.format_version);
  EXPECT_EQ(got.algorithm, want.algorithm);
  EXPECT_EQ(got.slice_count, kSlices);
  EXPECT_EQ(got.origin_block_size, want.origin_block_size);
  EXPECT_EQ(got.compressed_len, want.compressed_len);
  ASSERT_EQ(got_entries.size(), entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    EXPECT_EQ(got_entries[i].compressed_offset, entries[i].compressed_offset);
    EXPECT_EQ(got_entries[i].compressed_len, entries[i].compressed_len);
    EXPECT_EQ(got_entries[i].crc32, entries[i].crc32);
  }
}

TEST(HeaderTest, BudgetBoundaryAtOnePage) {
  // 248 条恰好一页（128 + 248×16 = 4096）；249 条超预算，写入拒绝。
  EXPECT_EQ(compress::SerializedHeaderSize(248), compress::kHeaderBudgetBytes);
  EXPECT_GT(compress::SerializedHeaderSize(249), compress::kHeaderBudgetBytes);

  const compress::FileHeader h = MakeHeader(249, 0);
  std::vector<compress::SliceEntry> entries(249);
  std::vector<std::uint8_t> buf(compress::SerializedHeaderSize(249));
  size_t len = 0;
  EXPECT_EQ(compress::SerializeHeader(h, entries.data(), 249, buf.data(), buf.size(), &len),
            compress::CompressErr::kInvalidArg);
}

TEST(HeaderTest, RejectsBadMagicTruncatedAndBadVersion) {
  const compress::FileHeader h = MakeHeader(2, 1024);
  std::vector<compress::SliceEntry> entries(2);
  std::vector<std::uint8_t> buf(compress::SerializedHeaderSize(2));
  size_t len = 0;
  ASSERT_EQ(compress::SerializeHeader(h, entries.data(), 2, buf.data(), buf.size(), &len),
            compress::CompressErr::kOk);

  compress::FileHeader got{};
  std::vector<compress::SliceEntry> got_entries;

  buf[0] = 'X';  // 魔数损坏 → 不是压缩文件
  EXPECT_EQ(compress::ParseHeader(buf.data(), buf.size(), &got, &got_entries),
            compress::CompressErr::kInvalidArg);
  buf[0] = 'C';

  EXPECT_EQ(compress::ParseHeader(buf.data(), 100, &got, &got_entries),
            compress::CompressErr::kBufSmall);  // 截断输入

  compress::FileHeader bad_version = MakeHeader(2, 1024);
  bad_version.format_version = 999;
  std::memcpy(buf.data(), &bad_version, compress::kFixedHeaderSize);
  EXPECT_EQ(compress::ParseHeader(buf.data(), buf.size(), &got, &got_entries),
            compress::CompressErr::kInvalidArg);
}

TEST(HeaderTest, LooksCompressedDetectsMagic) {
  const compress::FileHeader h = MakeHeader(1, 4096);
  std::vector<compress::SliceEntry> entries(1);
  std::vector<std::uint8_t> buf(compress::SerializedHeaderSize(1));
  size_t len = 0;
  ASSERT_EQ(compress::SerializeHeader(h, entries.data(), 1, buf.data(), buf.size(), &len),
            compress::CompressErr::kOk);

  EXPECT_TRUE(compress::LooksCompressed(buf.data()));
  buf[1] = 'X';
  EXPECT_FALSE(compress::LooksCompressed(buf.data()));
}

TEST(HeaderTest, EntryChainConsistency) {
  // 布局自检：S0.offset == 128 + 16N；逐条 offset[i]+len[i] == offset[i+1]。
  const std::uint32_t lens[] = {100, 200, 50};
  std::vector<compress::SliceEntry> entries = MakeChain(3, lens);


  EXPECT_EQ(entries[0].compressed_offset, compress::DataRegionOffset(3));
  EXPECT_EQ(entries[0].compressed_offset, 128u + 3 * 16u);
  EXPECT_EQ(entries[1].compressed_offset, entries[0].compressed_offset + 100u);
  EXPECT_EQ(entries[2].compressed_offset, entries[1].compressed_offset + 200u);
  EXPECT_EQ(compress::ValidateEntryChain(entries), compress::CompressErr::kOk);

  // 断链（offset 不衔接）必须被检出。
  entries[1].compressed_offset += 1;
  EXPECT_EQ(compress::ValidateEntryChain(entries), compress::CompressErr::kInvalidArg);

  // S0.offset 不等于数据区起点必须被检出。
  std::vector<compress::SliceEntry> bad_head = MakeChain(3, lens);
  bad_head[0].compressed_offset = 0;
  EXPECT_EQ(compress::ValidateEntryChain(bad_head), compress::CompressErr::kInvalidArg);

  EXPECT_EQ(compress::OriginOffset(2, compress::kDefaultSliceSize), 8u * 1024 * 1024);
  EXPECT_EQ(compress::OriginOffset(15, compress::kDefaultSliceSize), 60u * 1024 * 1024);
}

TEST(HeaderTest, SerializationIsDeterministic) {
  // 确定性契约：调用方结构体的 reserved / 对齐洞带垃圾，不进入输出——
  // 同语义字段两次序列化逐字节相同；带垃圾与干净结构体序列化结果也相同。
  constexpr std::uint32_t kSlices = 2;
  compress::FileHeader dirty = MakeHeader(kSlices, 8u * 1024 * 1024, 1500);
  std::memset(dirty.reserved, 0x7A, sizeof(dirty.reserved));  // 模拟 malloc 垃圾

  std::vector<compress::SliceEntry> entries(kSlices);
  entries[0] = {compress::DataRegionOffset(2), 1000, 0xDEADBEEF};
  entries[1] = {compress::DataRegionOffset(2) + 1000, 500, 0xCAFEBABE};

  compress::FileHeader clean = dirty;
  std::memset(clean.reserved, 0, sizeof(clean.reserved));

  std::vector<std::uint8_t> a(compress::SerializedHeaderSize(kSlices));
  std::vector<std::uint8_t> b(compress::SerializedHeaderSize(kSlices));
  size_t la = 0, lb = 0;
  ASSERT_EQ(compress::SerializeHeader(dirty, entries.data(), kSlices, a.data(), a.size(), &la),
            compress::CompressErr::kOk);
  ASSERT_EQ(compress::SerializeHeader(clean, entries.data(), kSlices, b.data(), b.size(), &lb),
            compress::CompressErr::kOk);
  ASSERT_EQ(la, lb);
  EXPECT_EQ(a, b);  // 逐字节相同：垃圾被值初始化挡在输出之外

  // 再序列化一次 dirty，结果仍稳定（非偶发）。
  std::vector<std::uint8_t> c(compress::SerializedHeaderSize(kSlices));
  size_t lc = 0;
  ASSERT_EQ(compress::SerializeHeader(dirty, entries.data(), kSlices, c.data(), c.size(), &lc),
            compress::CompressErr::kOk);
  EXPECT_EQ(a, c);
}
