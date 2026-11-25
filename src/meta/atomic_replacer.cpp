// 压缩结果落盘的唯一替换通道实现（ADR-0003 纪律）：
// 同卷专用临时目录（默认 <目标目录>/.tmp/）mkstemp → 写入 → fsync → 原子 rename；
// Open() 时 st_dev 比对保证 rename 不跨文件系统（跨卷 EXDEV fail-fast）；
// 析构与失败路径自动清理，不留半成品。
#include "compress/atomic_replacer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace compress {
namespace {

// 比较两个目录的 st_dev（内核文件系统编号）：不同卷 rename 必炸（EXDEV）。
bool SameFilesystem(const fs::path& a, const fs::path& b) noexcept {
  struct stat sa {};
  struct stat sb {};
  if (::stat(a.c_str(), &sa) != 0 || ::stat(b.c_str(), &sb) != 0) {
    return false;
  }
  return sa.st_dev == sb.st_dev;
}

}  // namespace

AtomicReplacer::AtomicReplacer(std::string target_path, std::string temp_dir)
    : target_path_(std::move(target_path)), temp_dir_(std::move(temp_dir)) {}

AtomicReplacer::~AtomicReplacer() noexcept {
  if (!committed_) {
    Abort();
  }
}

CompressErr AtomicReplacer::Open() noexcept {
  if (fd_ >= 0) {
    return CompressErr::kWrongState;  // 已开一轮：先 Commit/Abort 再重开
  }

  // 1) 解析临时目录：显式配置优先，否则 <目标目录>/.tmp/
  fs::path target(target_path_);
  fs::path parent = target.parent_path();
  if (parent.empty()) {
    parent = ".";  // 相对裸文件名：目标即当前目录
  }
  const fs::path tdir = temp_dir_.empty() ? parent / ".tmp" : fs::path(temp_dir_);

  // 2) 目标目录必须已存在（mkdir -p 目标目录是调用方的职责，不越权补）
  std::error_code ec;
  if (!fs::is_directory(parent, ec)) {
    return CompressErr::kIo;
  }

  // 3) 临时目录不存在则创建（HDFS 模式：卷内专用目录）
  fs::create_directories(tdir, ec);
  if (ec) {
    return CompressErr::kIo;
  }

  // 4) 跨卷 fail-fast：rename 跨文件系统返回 EXDEV，提前拦截（防误配 /tmp/tmpfs）
  if (!SameFilesystem(parent, tdir)) {
    return CompressErr::kIo;
  }

  // 5) mkstemp 原地改写模板尾部，需可写缓冲
  const std::string tmpl_path = (tdir / target.filename()).string();
  std::vector<char> tmpl(tmpl_path.begin(), tmpl_path.end());
  tmpl.insert(tmpl.end(), {'.'});
  tmpl.insert(tmpl.end(), {'t', 'm', 'p', '.', 'X', 'X', 'X', 'X', 'X', 'X'});
  tmpl.push_back('\0');

  const int fd = ::mkstemp(tmpl.data());
  if (fd < 0) {
    return CompressErr::kIo;
  }
  fd_ = fd;
  temp_path_.assign(tmpl.data());
  committed_ = false;  // 新一轮：上轮 Commit 的成功标记不得带入（否则析构跳过本轮清理）
  return CompressErr::kOk;
}

CompressErr AtomicReplacer::Commit() noexcept {
  if (fd_ < 0) {
    return CompressErr::kWrongState;  // 未 Open：无可提交内容
  }
  // 失败分支统一由 Abort() 收口（close+unlink）：不在此处自行清 fd_，
  // 否则短路求值下 fsync 失败会漏掉 close，造成描述符泄漏。
  if (::fsync(fd_) != 0) {
    Abort();
    return CompressErr::kIo;
  }
  if (::close(fd_) != 0) {
    fd_ = -1;  // close 失败：描述符已不可再操作
    Abort();
    return CompressErr::kIo;
  }
  fd_ = -1;
  if (::rename(temp_path_.c_str(), target_path_.c_str()) != 0) {
    Abort();
    return CompressErr::kIo;
  }
  temp_path_.clear();
  committed_ = true;  // 目标已替换，析构不再触碰
  return CompressErr::kOk;
}

void AtomicReplacer::Abort() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (!temp_path_.empty()) {
    ::unlink(temp_path_.c_str());
    temp_path_.clear();
  }
}

}  // namespace compress
