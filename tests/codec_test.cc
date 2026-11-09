// 票 06：统一编解码封装单测。round-trip 断言逐字节相等。
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "compress/codec.h"

namespace {

std::vector<std::uint8_t> RandomBytes(size_t n, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::vector<std::uint8_t> v(n);
    std::generate(v.begin(), v.end(), [&] { return static_cast<std::uint8_t>(rng()); });
    return v;
}

std::vector<std::uint8_t> RepetitiveBytes(size_t n) {
    const char unit[] = "2026-11-09 INFO request handled ok\n";
    std::vector<std::uint8_t> v;
    v.reserve(n);
    while (v.size() < n) {
        for (const char* p = unit; *p != '\0' && v.size() < n; ++p) {
            v.push_back(static_cast<std::uint8_t>(*p));
        }
    }
    return v;
}

std::unique_ptr<compress::ICodec> MakeCodec(compress::CompressLevel level) {
    std::unique_ptr<compress::ICodec> codec = compress::CodecFactory::Create(level);
    EXPECT_TRUE(codec != nullptr);
    return codec;
}

// ---- 参数化：两种压缩级别跑同一套行为契约（TEST_P ≈ pytest parametrize）----
class CodecContractTest : public ::testing::TestWithParam<compress::CompressLevel> {};

INSTANTIATE_TEST_SUITE_P(BothLevels, CodecContractTest,
                         ::testing::Values(compress::CompressLevel::kShallow,
                                           compress::CompressLevel::kDeep));

TEST_P(CodecContractTest, RandomInputRoundTrip) {
    const std::vector<std::uint8_t> input = RandomBytes(1 << 20, 42);
    const size_t cap = input.size() * 2;  // 2× 原始大小纪律（设计文档 §3.1.3）
    std::unique_ptr<compress::ICodec> codec = MakeCodec(GetParam());

    std::vector<std::uint8_t> compressed(cap);
    size_t out_len = 0;
    compress::CompressErr err = compress::CompressErr::kOk;
    const size_t n = codec->Compress(input.data(), input.size(), compressed.data(),
                                     cap, &out_len, &err);
    ASSERT_EQ(err, compress::CompressErr::kOk);
    ASSERT_GT(n, 0u);

    std::vector<std::uint8_t> restored(input.size());
    size_t restored_len = 0;
    err = compress::CompressErr::kOk;
    const size_t rn = codec->Decompress(compressed.data(), n, restored.data(),
                                        restored.size(), &restored_len, &err);
    ASSERT_EQ(err, compress::CompressErr::kOk);
    ASSERT_EQ(rn, input.size());
    ASSERT_EQ(restored_len, input.size());
    EXPECT_EQ(restored, input);  // 逐字节相等
}

TEST_P(CodecContractTest, RepetitiveInputRoundTrip) {
    const std::vector<std::uint8_t> input = RepetitiveBytes(4 << 20);
    std::unique_ptr<compress::ICodec> codec = MakeCodec(GetParam());

    std::vector<std::uint8_t> compressed(input.size() * 2);
    size_t n = 0;
    compress::CompressErr err = compress::CompressErr::kOk;
    n = codec->Compress(input.data(), input.size(), compressed.data(),
                        compressed.size(), &n, &err);
    ASSERT_EQ(err, compress::CompressErr::kOk);

    std::vector<std::uint8_t> restored(input.size());
    size_t got = 0;
    err = compress::CompressErr::kOk;
    const size_t rn = codec->Decompress(compressed.data(), n, restored.data(),
                                        restored.size(), &got, &err);
    ASSERT_EQ(err, compress::CompressErr::kOk);
    EXPECT_EQ(restored, input);
    EXPECT_EQ(rn, input.size());
}

TEST_P(CodecContractTest, CompressSmallBufferReportsNeededSize) {
    // kBufSmall 契约：*out_len 带回所需容量，扩容重调一次即成功。
    // 压缩侧容量 = 调用方（server/压缩任务）按纪律给的缓冲，与解压侧语义不同。
    const std::vector<std::uint8_t> input = RepetitiveBytes(1 << 20);
    std::unique_ptr<compress::ICodec> codec = MakeCodec(GetParam());

    std::vector<std::uint8_t> tiny(16);
    size_t need = 0;
    compress::CompressErr err = compress::CompressErr::kOk;
    const size_t n = codec->Compress(input.data(), input.size(), tiny.data(),
                                     tiny.size(), &need, &err);
    EXPECT_EQ(n, 0u);
    EXPECT_EQ(err, compress::CompressErr::kBufSmall);
    EXPECT_GT(need, tiny.size());

    std::vector<std::uint8_t> enough(need);
    err = compress::CompressErr::kOk;
    EXPECT_GT(codec->Compress(input.data(), input.size(), enough.data(),
                              enough.size(), &need, &err),
              0u);
    EXPECT_EQ(err, compress::CompressErr::kOk);
}

TEST_P(CodecContractTest, CorruptDataFails) {
    // 翻转中段字节再解压：必须失败或产出不等，不能静默给错数据。
    // 两侧各有校验证据：LZ4 帧内容校验和（压缩侧开启），LZMA CRC64（LZMA_CHECK_CRC64）。
    const std::vector<std::uint8_t> input = RepetitiveBytes(1 << 20);
    std::unique_ptr<compress::ICodec> codec = MakeCodec(GetParam());

    std::vector<std::uint8_t> compressed(input.size() * 2);
    size_t n = 0;
    compress::CompressErr err = compress::CompressErr::kOk;
    n = codec->Compress(input.data(), input.size(), compressed.data(),
                        compressed.size(), &n, &err);
    ASSERT_EQ(err, compress::CompressErr::kOk);

    compressed[n / 2] ^= 0xFF;
    std::vector<std::uint8_t> out(input.size());
    size_t got = 0;
    err = compress::CompressErr::kOk;
    const size_t rn = codec->Decompress(compressed.data(), n, out.data(), out.size(), &got, &err);
    EXPECT_TRUE(err != compress::CompressErr::kOk || rn != input.size() || out != input);
}

TEST_P(CodecContractTest, EmptyInputRejected) {
    // 领域语义：块/切片按定义不为空（切片 4MB 定长，块由文件切出）。
    // 空输入钉为 kInvalidArg——「不该发生」比「成功压出空流」更诚实。
    std::unique_ptr<compress::ICodec> codec = MakeCodec(GetParam());
    std::vector<std::uint8_t> buf(64);
    size_t n = 0;
    compress::CompressErr err = compress::CompressErr::kOk;

    EXPECT_EQ(codec->Compress(buf.data(), 0, buf.data(), buf.size(), &n, &err), 0u);
    EXPECT_EQ(err, compress::CompressErr::kInvalidArg);

    err = compress::CompressErr::kOk;
    EXPECT_EQ(codec->Decompress(buf.data(), 0, buf.data(), buf.size(), &n, &err), 0u);
    EXPECT_EQ(err, compress::CompressErr::kInvalidArg);
}

TEST_P(CodecContractTest, NullArgsRejected) {
    // 对外承诺只有「不崩 + kInvalidArg」，不承诺内部哪一步检查（实现可变，契约不变）。
    std::unique_ptr<compress::ICodec> codec = MakeCodec(GetParam());
    std::uint8_t buf[16] = {};
    size_t n = 0;
    compress::CompressErr err = compress::CompressErr::kOk;

    EXPECT_EQ(codec->Compress(nullptr, 8, buf, sizeof(buf), &n, &err), 0u);
    EXPECT_EQ(err, compress::CompressErr::kInvalidArg);
    EXPECT_EQ(codec->Decompress(nullptr, 8, buf, sizeof(buf), &n, &err), 0u);
    EXPECT_EQ(err, compress::CompressErr::kInvalidArg);

    const std::vector<std::uint8_t> input = RepetitiveBytes(1024);
    std::vector<std::uint8_t> compressed(4096);
    err = compress::CompressErr::kOk;
    ASSERT_GT(codec->Compress(input.data(), input.size(), compressed.data(),
                              compressed.size(), &n, &err),
              0u);
    err = compress::CompressErr::kOk;
    EXPECT_EQ(codec->Decompress(compressed.data(), n, nullptr, 1024, &n, &err), 0u);
    EXPECT_EQ(err, compress::CompressErr::kInvalidArg);
}

}  // namespace

TEST(CodecFactoryTest, LevelSelectsAlgorithm) {
    EXPECT_TRUE(compress::CodecFactory::Create(compress::CompressLevel::kShallow) != nullptr);
    EXPECT_TRUE(compress::CodecFactory::Create(compress::CompressLevel::kDeep) != nullptr);
}

TEST(CodecFactoryTest, ShallowTradesRatioForCpu) {
    // 级别 → 算法映射的行为证据：同一份重复文本，LZ4 浅压产出应显著大于 LZMA 深压。
    const std::vector<std::uint8_t> input = RepetitiveBytes(1 << 20);
    std::unique_ptr<compress::ICodec> shallow =
        compress::CodecFactory::Create(compress::CompressLevel::kShallow);
    std::unique_ptr<compress::ICodec> deep =
        compress::CodecFactory::Create(compress::CompressLevel::kDeep);

    std::vector<std::uint8_t> a(input.size() * 2), b(input.size() * 2);
    size_t al = 0, bl = 0;
    compress::CompressErr ae = compress::CompressErr::kOk, be = compress::CompressErr::kOk;
    const size_t an = shallow->Compress(input.data(), input.size(), a.data(), a.size(), &al, &ae);
    const size_t bn = deep->Compress(input.data(), input.size(), b.data(), b.size(), &bl, &be);
    ASSERT_EQ(ae, compress::CompressErr::kOk);
    ASSERT_EQ(be, compress::CompressErr::kOk);
    EXPECT_LT(an, input.size() / 2);  // 重复文本浅压至少省一半
    EXPECT_GT(an, bn);                // 浅压比率劣于深压 = 用空间换 CPU
}

TEST(CodecTest, Lz4DecompressSmallBufferReportsSize) {
    // 【W3 功能验证用例】解压侧容量不足 → kBufSmall 报所需容量。
    // 注意：真实读路径的期望大小来自文件头 origin_block_size（issue 02-D），
    // 不依赖 codec 特性；此用例验证编解码层自身契约，W5 接入文件头后语义需按设计文档复核。
    // LZMA 侧暂缓：xz 格式的原始大小在流尾索引，one-shot API 不暴露，同理归 W5。
    const std::vector<std::uint8_t> input = RepetitiveBytes(1 << 20);
    std::unique_ptr<compress::ICodec> codec =
        compress::CodecFactory::Create(compress::CompressLevel::kShallow);

    std::vector<std::uint8_t> compressed(input.size() * 2);
    size_t n = 0;
    compress::CompressErr err = compress::CompressErr::kOk;
    n = codec->Compress(input.data(), input.size(), compressed.data(),
                        compressed.size(), &n, &err);
    ASSERT_EQ(err, compress::CompressErr::kOk);

    std::vector<std::uint8_t> half(input.size() / 2);
    size_t need = 0;
    err = compress::CompressErr::kOk;
    const size_t rn = codec->Decompress(compressed.data(), n, half.data(),
                                        half.size(), &need, &err);
    EXPECT_EQ(rn, 0u);
    EXPECT_EQ(err, compress::CompressErr::kBufSmall);
    EXPECT_EQ(need, input.size());  // LZ4 帧 contentSize 精确报出

    std::vector<std::uint8_t> enough(need);
    err = compress::CompressErr::kOk;
    EXPECT_EQ(codec->Decompress(compressed.data(), n, enough.data(),
                                enough.size(), &need, &err),
              input.size());
    EXPECT_EQ(err, compress::CompressErr::kOk);
}
