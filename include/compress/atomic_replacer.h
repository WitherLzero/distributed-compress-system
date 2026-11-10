// 压缩结果落盘的唯一替换通道（ADR-0003 纪律）：临时文件 + 原子 rename。
// 全系统只此一处执行替换，其余代码不直接 rename；
// 临时文件与目标同目录（rename 不跨文件系统）；析构兜底清理，失败不留半成品。
#pragma once

#include <string>

#include "compress/error_code.h"

namespace compress {

class AtomicReplacer {
public:
    explicit AtomicReplacer(std::string target_path);
    ~AtomicReplacer() noexcept;  // 未 Commit 成功则自动清理临时文件（RAII 防半成品）

    AtomicReplacer(const AtomicReplacer&) = delete;
    AtomicReplacer& operator=(const AtomicReplacer&) = delete;

    /**
     * @brief 在目标同目录创建临时文件（<target>.tmp.XXXXXX），打开写通道。
     * @return kOk = 临时文件就绪，Fd() 可写；kIo = 目录不存在/权限不足等文件系统错误
     */
    CompressErr Open() noexcept;

    int Fd() const noexcept { return fd_; }  // 临时文件描述符，供写入（pwrite）；未 Open 为 -1

    /**
     * @brief 提交替换：fsync 落盘后原子 rename 覆盖目标。
     * @return kOk = 目标已被替换；kWrongState = 未 Open；kIo = 落盘或 rename 失败
     *              （失败时自动清理临时文件，可安全重开新一轮）
     */
    CompressErr Commit() noexcept;

    void Abort() noexcept;  // 主动放弃：关闭并删除临时文件（幂等）

private:
    std::string target_path_;
    std::string temp_path_;  // 空串 = 当前无未决临时文件
    int fd_ = -1;
    bool committed_ = false;
};

}  // namespace compress
