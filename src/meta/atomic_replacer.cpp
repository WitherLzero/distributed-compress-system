// 压缩结果落盘的唯一替换通道（ADR-0003 纪律，票 08 实写）：
// 同目录临时文件（mkstemp）→ 写入 → fsync → 原子 rename；析构与失败路径自动清理，不留半成品。
#include "compress/atomic_replacer.h"

#include <fcntl.h>
#include <unistd.h>

#include <vector>

namespace compress {

AtomicReplacer::AtomicReplacer(std::string target_path) : target_path_(std::move(target_path)) {}

AtomicReplacer::~AtomicReplacer() noexcept {
  if (!committed_) {
    Abort();
  }
}

CompressErr AtomicReplacer::Open() noexcept {
  if (fd_ >= 0) {
    return CompressErr::kWrongState;  // 已开一轮：先 Commit/Abort 再重开
  }
  // mkstemp 原地改写模板尾部，需可写缓冲；临时文件与目标同目录保证 rename 同文件系统。
  std::vector<char> tmpl(target_path_.begin(), target_path_.end());
  tmpl.push_back('.');
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
