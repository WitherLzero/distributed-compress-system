// 冷数据扫描线程主体：周期读进程内存中的 block 元信息（atime，不遍历文件系统），
// 产出候选压缩任务入队（设计文档 §7.2、§3.1.4）。
#pragma once

namespace compress {

class ColdScanner {
public:
    ColdScanner() = default;

    void Start(int interval_ms);  // 启动扫描线程，按周期触发 ScanOnce
    void ScanOnce();              // 单轮：读内存 block 元信息(atime) → 产出候选 → 入队
};

}  // namespace compress
