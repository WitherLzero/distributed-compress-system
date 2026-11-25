// load_sampler_test —— 心跳负载取样单测：CPU 增量数学 + MEM 余量占比。
// reader 注入构造的 /proc 内容，不依赖真实机器负载（测试确定性）。
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "load_sampler.h"

namespace {

using namespace compress;

// 构造一行 /proc/stat：user nice system idle iowait irq softirq steal
std::string StatLine(long long user, long long nice, long long system, long long idle,
                     long long iowait = 0, long long irq = 0, long long softirq = 0,
                     long long steal = 0) {
  return "cpu " + std::to_string(user) + " " + std::to_string(nice) + " " +
         std::to_string(system) + " " + std::to_string(idle) + " " + std::to_string(iowait) +
         " " + std::to_string(irq) + " " + std::to_string(softirq) + " " +
         std::to_string(steal);
}

std::string MemInfo(long long total_kb, long long available_kb) {
  std::ostringstream ss;
  ss << "MemTotal:       " << total_kb << " kB\n"
     << "MemAvailable:   " << available_kb << " kB\n";
  return ss.str();
}

// 1) 首次采样无上值可差分：诚实返回 0，不造数
TEST(LoadSamplerTest, FirstCpuSampleReturnsZero) {
  LoadSampler s([](const char*) { return StatLine(100, 0, 0, 100); });
  EXPECT_DOUBLE_EQ(s.CpuUtilization(), 0.0);
}

// 2) 增量数学：Δbusy / Δtotal；idle 与 iowait 均不计入 busy
TEST(LoadSamplerTest, CpuDeltaComputesUtilization) {
  std::string line = StatLine(100, 0, 0, 100);  // total=200 busy=100
  LoadSampler s([&line](const char*) { return line; });
  s.CpuUtilization();  // 建立基线

  line = StatLine(150, 0, 0, 150);  // total=300 busy=150 → Δbusy=50 Δtotal=100
  EXPECT_DOUBLE_EQ(s.CpuUtilization(), 50.0);

  line = StatLine(150, 0, 0, 250);  // total=400 busy=150 → 全在 idle 上 → 0
  EXPECT_DOUBLE_EQ(s.CpuUtilization(), 0.0);
}

// 3) iowait 不算忙：busy 口径 = total − idle − iowait
TEST(LoadSamplerTest, IoWaitIsNotBusy) {
  std::string line = StatLine(50, 0, 50, 100, 0);  // total=200 busy=100
  LoadSampler s([&line](const char*) { return line; });
  s.CpuUtilization();

  line = StatLine(50, 0, 50, 100, 100);  // 增量全落在 iowait → 利用率 0
  EXPECT_DOUBLE_EQ(s.CpuUtilization(), 0.0);
}

// 4) 系统已用占比：MemTotal − MemAvailable
TEST(LoadSamplerTest, MemUtilizationComputesUsedRatio) {
  LoadSampler s([](const char*) { return MemInfo(1000, 250); });
  EXPECT_DOUBLE_EQ(s.MemUtilization(), 75.0);
}

// 5) 内容损坏不炸不造数：返回 0
TEST(LoadSamplerTest, MalformedInputReturnsZero) {
  LoadSampler s([](const char*) { return "garbage line\n"; });
  EXPECT_DOUBLE_EQ(s.CpuUtilization(), 0.0);
  EXPECT_DOUBLE_EQ(s.MemUtilization(), 0.0);
}

}  // namespace
