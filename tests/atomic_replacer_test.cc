// 票 08：AtomicReplacer 单测（issue 03 承诺的 replacer 单测补落）。
// 验证契约：同目录临时文件 → fsync+rename 原子替换；未 Commit 的析构/Abort 不留半成品。
#include "compress/atomic_replacer.h"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using compress::AtomicReplacer;
using compress::CompressErr;

namespace {

// 每个测试独立临时目录（mkdtemp），作用域结束整树删除，互不污染、可重复运行。
class TempDir {
 public:
  TempDir() {
    std::string tmpl = "/tmp/replacer_test.XXXXXX";
    char* dir = ::mkdtemp(tmpl.data());
    if (dir == nullptr) {
      // 测试基建失败（非被测行为）：ASSERT 在构造函数内无法 return，直接终止。
      std::fprintf(stderr, "mkdtemp failed\n");
      std::abort();
    }
    path_ = dir;
  }
  ~TempDir() { fs::remove_all(path_); }

  fs::path path() const { return path_; }

 private:
  fs::path path_;
};

std::string ReadAll(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void WriteAll(int fd, const std::string& data) {
  std::size_t off = 0;
  while (off < data.size()) {
    const ssize_t n = ::write(fd, data.data() + off, data.size() - off);
    ASSERT_GT(n, 0);
    off += static_cast<std::size_t>(n);
  }
}

std::size_t FileCount(const fs::path& dir) {
  return static_cast<std::size_t>(
      std::distance(fs::directory_iterator(dir), fs::directory_iterator{}));
}

}  // namespace

TEST(AtomicReplacerTest, CommitReplacesTarget) {
  TempDir dir;
  const fs::path target = dir.path() / "blk_101";
  { std::ofstream(target, std::ios::binary) << "old_content"; }

  AtomicReplacer replacer(target.string());
  ASSERT_EQ(replacer.Open(), CompressErr::kOk);
  ASSERT_GE(replacer.Fd(), 0);
  WriteAll(replacer.Fd(), "new_compressed_payload");
  ASSERT_EQ(replacer.Commit(), CompressErr::kOk);

  EXPECT_EQ(ReadAll(target), "new_compressed_payload");  // 目标已被替换
  EXPECT_EQ(FileCount(dir.path()), 1u);                  // 无临时文件残留
}

TEST(AtomicReplacerTest, CommitWithoutOpenFails) {
  TempDir dir;
  AtomicReplacer replacer((dir.path() / "blk").string());
  EXPECT_EQ(replacer.Commit(), CompressErr::kWrongState);  // 未 Open 无可提交内容
}

TEST(AtomicReplacerTest, ReopenWithoutFinishFails) {
  TempDir dir;
  AtomicReplacer replacer((dir.path() / "blk").string());
  ASSERT_EQ(replacer.Open(), CompressErr::kOk);
  EXPECT_EQ(replacer.Open(), CompressErr::kWrongState);  // 一轮未了结前不得重开
  replacer.Abort();
}

TEST(AtomicReplacerTest, AbortRemovesTempAndKeepsTarget) {
  TempDir dir;
  const fs::path target = dir.path() / "blk_101";
  { std::ofstream(target, std::ios::binary) << "original"; }

  AtomicReplacer replacer(target.string());
  ASSERT_EQ(replacer.Open(), CompressErr::kOk);
  WriteAll(replacer.Fd(), "half_written_garbage");
  replacer.Abort();

  EXPECT_EQ(ReadAll(target), "original");  // 目标原样未动
  EXPECT_EQ(FileCount(dir.path()), 1u);    // 临时文件已清除
}

TEST(AtomicReplacerTest, DestructorAbortsUncommitted) {
  TempDir dir;
  const fs::path target = dir.path() / "blk_101";
  {
    AtomicReplacer replacer(target.string());
    ASSERT_EQ(replacer.Open(), CompressErr::kOk);
    WriteAll(replacer.Fd(), "uncommitted");
    // 不 Commit 不 Abort，直接出作用域：RAII 兜底清理
  }
  EXPECT_FALSE(fs::exists(target));      // 目标从未被触碰
  EXPECT_EQ(FileCount(dir.path()), 0u);  // 连目标带临时文件都不留
}

TEST(AtomicReplacerTest, OpenInMissingDirFails) {
  AtomicReplacer replacer("/nonexistent_dir_zzz/blk");
  EXPECT_EQ(replacer.Open(), CompressErr::kIo);  // 目录不存在：干净失败，不崩
}

// 回归（评审 A2）：成功 Commit 后重开新一轮，新轮未 Commit 即析构——
// 上一轮 committed_ 的成功标记不得让析构跳过新一轮清理。
TEST(AtomicReplacerTest, ReopenAfterCommitCleansUp) {
  TempDir dir;
  const fs::path target = dir.path() / "blk_101";
  {
    AtomicReplacer replacer(target.string());
    ASSERT_EQ(replacer.Open(), CompressErr::kOk);
    WriteAll(replacer.Fd(), "round1");
    ASSERT_EQ(replacer.Commit(), CompressErr::kOk);

    ASSERT_EQ(replacer.Open(), CompressErr::kOk);  // 头注释承诺：可安全重开新一轮
    WriteAll(replacer.Fd(), "round2");
    // 新一轮不 Commit 不 Abort，直接出作用域。
  }
  EXPECT_EQ(FileCount(dir.path()), 1u);  // 只有 round1 落盘的目标，无临时文件残留
  EXPECT_EQ(ReadAll(target), "round1");
}
