// 票 06：块级文件头读写单测。布局按已定稿决策：128B 定长头 + 8B 条目，合计 ≤ 4KB。
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "compress/compressed_file_header.h"

namespace {

compress::FileHeader MakeHeader(std::uint32_t slice_count, std::uint64_t origin_size) {
    compress::FileHeader h{};
    std::memcpy(h.magic, "CPS1", 4);
    h.format_version = compress::kFormatVersion;
    h.algorithm = 1;  // LZMA 深压
    h.flags = 0;
    h.slice_count = slice_count;
    h.origin_block_size = origin_size;
    return h;
}

}  // namespace

TEST(HeaderTest, SerializedSizeIs128Plus8PerSlice) {
    EXPECT_EQ(compress::SerializedHeaderSize(0), 128u);
    EXPECT_EQ(compress::SerializedHeaderSize(3), 128u + 3 * 8u);
    EXPECT_EQ(compress::SerializedHeaderSize(16), 256u);  // 64MB/4MB 整块
}

TEST(HeaderTest, WriteReadRoundTrip) {
    constexpr std::uint32_t kSlices = 3;
    const compress::FileHeader want = MakeHeader(kSlices, 10u * 1024 * 1024);
    std::vector<compress::SliceEntry> entries(kSlices);
    entries[0] = {900 * 1024, 0xAABBCCDD};
    entries[1] = {1100 * 1024, 0x11223344};
    entries[2] = {800 * 1024, 0x55667788};

    std::vector<std::uint8_t> buf(compress::SerializedHeaderSize(kSlices));
    size_t len = 0;
    ASSERT_EQ(compress::SerializeHeader(want, entries.data(), kSlices,
                                        buf.data(), buf.size(), &len),
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
    ASSERT_EQ(got_entries.size(), entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        EXPECT_EQ(got_entries[i].compressed_len, entries[i].compressed_len);
        EXPECT_EQ(got_entries[i].crc32, entries[i].crc32);
    }
}

TEST(HeaderTest, BudgetBoundaryAtOnePage) {
    // 496 条恰好一页；497 条超预算，写入拒绝。
    EXPECT_EQ(compress::SerializedHeaderSize(496), compress::kHeaderBudgetBytes);
    EXPECT_GT(compress::SerializedHeaderSize(497), compress::kHeaderBudgetBytes);

    const compress::FileHeader h = MakeHeader(497, 0);
    std::vector<compress::SliceEntry> entries(497);
    std::vector<std::uint8_t> buf(compress::SerializedHeaderSize(497));
    size_t len = 0;
    EXPECT_EQ(compress::SerializeHeader(h, entries.data(), 497, buf.data(), buf.size(), &len),
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

TEST(HeaderTest, DerivedOffsetsFromOrderAndFixedSize) {
    // 不存盘的两个坐标：压缩偏移 = 数据区起点 + 前缀和；原始偏移 = i × 切片大小。
    std::vector<compress::SliceEntry> entries(3);
    entries[0].compressed_len = 100;
    entries[1].compressed_len = 200;
    entries[2].compressed_len = 50;

    EXPECT_EQ(compress::DataRegionOffset(3), 128u + 3 * 8u);
    EXPECT_EQ(compress::CompressedOffset(entries, 0), 152u);
    EXPECT_EQ(compress::CompressedOffset(entries, 1), 252u);
    EXPECT_EQ(compress::CompressedOffset(entries, 2), 452u);
    EXPECT_EQ(compress::CompressedOffset(entries, 3), 502u);

    EXPECT_EQ(compress::OriginOffset(2, compress::kDefaultSliceSize), 8u * 1024 * 1024);
    EXPECT_EQ(compress::OriginOffset(15, compress::kDefaultSliceSize), 60u * 1024 * 1024);
}
