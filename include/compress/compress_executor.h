// 压缩工作线程主体：取任务 → 内存缓存列表 random 选点 → 经 IRpcAdapter 发压缩 →
// temp 文件 + 原子 rename 回写（设计文档 §7.2、§3.1.3）。
#pragma once

namespace compress {

class CompressExecutor {
public:
    CompressExecutor() = default;

    void Run();        // 工作线程主循环：Pop 任务直至队列关闭且排干
    void OnRpcFail();  // 退避后重选实例（复用已读缓冲区，见 §5.3）
};

}  // namespace compress
