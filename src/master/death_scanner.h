// compress_master 侧骨架（票 08）：判死扫描线程。
// 周期 1~2s 本地遍历注册表时间戳，超判死阈值（心跳间隔 × 倍数，当前 4×5s=20s）剔除；
// 纯本地操作，不发 RPC（设计文档 §7.2）。
#pragma once

namespace compress {

class DeathScanner {
public:
    DeathScanner() = default;

    void Run();  // 扫描线程主循环
};

}  // namespace compress
