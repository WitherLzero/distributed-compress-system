// compress_server 侧（票 06 / CP6）：心跳负载取样。
// CPU = 系统级利用率（/proc/stat 相邻采样 busy 增量占比，节点余量视角——
// 真实部署一台机上不只有 compress_server，进程自身 utime 会漏掉竞争者）；
// MEM = 系统已用占比（(MemTotal − MemAvailable) / MemTotal）。
// reader 可注入：默认读真实 /proc，单测喂构造内容断言增量数学。
#pragma once

#include <functional>
#include <string>

namespace compress {

class LoadSampler {
 public:
  using Reader = std::function<std::string(const char* path)>;

  explicit LoadSampler(Reader reader = RealReader);

  /// 相邻两次调用间的系统 busy 增量占比 [0,100]；首次调用无上值，返回 0.0（不造数）。
  double CpuUtilization();
  /// 系统已用内存占比 [0,100]；读取失败返回 0.0。
  double MemUtilization();

 private:
  static std::string RealReader(const char* path);

  Reader reader_;
  long long prev_total_ = -1;  // /proc/stat 为开机累计值，利用率必须差分
  long long prev_busy_ = -1;
};

}  // namespace compress
