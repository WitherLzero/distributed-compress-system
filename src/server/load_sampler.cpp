#include "load_sampler.h"

#include <fstream>
#include <sstream>

namespace compress {

namespace {

// busy 口径 = total − idle − iowait（与 htop 的 CPU% 一致）：
// 等待 IO 不算忙，idle 也不算。
bool ParseStat(const std::string& line, long long* total, long long* busy) {
  std::istringstream iss(line);
  std::string tag;
  if (!(iss >> tag) || tag != "cpu") {
    return false;
  }
  long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0,
            steal = 0;
  if (!(iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
    return false;
  }
  *total = user + nice + system + idle + iowait + irq + softirq + steal;
  *busy = *total - idle - iowait;
  return true;
}

bool ParseMeminfoField(const std::string& content, const char* key, long long* value_kb) {
  std::istringstream iss(content);
  std::string line;
  const std::string prefix = key;
  while (std::getline(iss, line)) {
    if (line.compare(0, prefix.size(), prefix) == 0) {
      std::istringstream ls(line);
      std::string name, unit;
      long long v = 0;
      if (ls >> name >> v >> unit && unit == "kB") {
        *value_kb = v;
        return true;
      }
      return false;
    }
  }
  return false;
}

}  // namespace

std::string LoadSampler::RealReader(const char* path) {
  std::ifstream f(path);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

LoadSampler::LoadSampler(Reader reader) : reader_(std::move(reader)) {}

double LoadSampler::CpuUtilization() {
  std::istringstream iss(reader_("/proc/stat"));
  std::string first_line;
  if (!std::getline(iss, first_line)) {
    return 0.0;
  }
  long long total = 0, busy = 0;
  if (!ParseStat(first_line, &total, &busy)) {
    return 0.0;
  }
  if (prev_total_ < 0) {
    prev_total_ = total;
    prev_busy_ = busy;
    return 0.0;  // 首次采样：建立基线，无上值可差分——诚实返回 0
  }
  const long long d_total = total - prev_total_;
  const long long d_busy = busy - prev_busy_;
  prev_total_ = total;
  prev_busy_ = busy;
  if (d_total <= 0) {  // 计数器未前进（极小概率）或内容异常：不造数
    return 0.0;
  }
  return 100.0 * static_cast<double>(d_busy) / static_cast<double>(d_total);
}

double LoadSampler::MemUtilization() {
  const std::string content = reader_("/proc/meminfo");
  long long total_kb = 0, available_kb = 0;
  if (!ParseMeminfoField(content, "MemTotal:", &total_kb) ||
      !ParseMeminfoField(content, "MemAvailable:", &available_kb) || total_kb <= 0) {
    return 0.0;
  }
  const long long used = total_kb - available_kb;
  const long long clamped = used < 0 ? 0 : used;
  return 100.0 * static_cast<double>(clamped) / static_cast<double>(total_kb);
}

}  // namespace compress
