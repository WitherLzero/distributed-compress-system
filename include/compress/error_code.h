// 自产错误码：对外语义见设计文档 §6.2.2。
// §5.5 业务层失败（数据不可读/为空/超限/压缩失败/收益不足）为库内日志原因，不上抛宿主。
#pragma once

#include <cstdint>

namespace compress {

enum class CompressErr : std::uint8_t {
    kOk = 0,
    kInvalidArg,  // 参数非法
    kWrongState,  // 调用顺序错（未 Start 就 Read 等）
    kIo,          // 本地文件系统错误
    kBufSmall,    // 缓冲不足，out_len 已带回所需容量，扩容重调一次
    kBusy,        // 解压通道饱和，稍后重试
};

inline const char* ToString(CompressErr e) noexcept {
    switch (e) {
        case CompressErr::kOk:          return "kOk";
        case CompressErr::kInvalidArg:  return "kInvalidArg";
        case CompressErr::kWrongState:  return "kWrongState";
        case CompressErr::kIo:          return "kIo";
        case CompressErr::kBufSmall:    return "kBufSmall";
        case CompressErr::kBusy:        return "kBusy";
    }
    return "unknown";
}

}  // namespace compress
