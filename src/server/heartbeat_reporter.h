// compress_server 侧骨架（票 08）：心跳上报线程。
// 启动即发首跳（注册合并），此后 5s 周期上报 queue_len/cpu/mem（设计文档 §7.2）。
#pragma once

namespace compress {

class HeartbeatReporter {
 public:
  HeartbeatReporter() = default;

  void Run();  // 心跳线程主循环
};

}  // namespace compress
