// 库配置结构体：全部可配置项经 Options 值注入（§6.2.1）。
// 数值默认值均为占位，联调期标定（§5.6）；库内禁止 gflags（§6.2.3 边界纪律 2）。
#pragma once

#include <string>

namespace compress {

struct Options {
  std::string data_dir;                        // 扫描根目录（块文件所在）
  std::string master_addr = "127.0.0.1:8001";  // master 地址
  int scan_interval_sec = 30;                  // 冷数据扫描周期
  double min_saving_ratio = 0.10;  // 整块节省率阈值，低于则放弃压缩（§5.4）
  std::string temp_dir;            // AtomicReplacer 临时目录；空 = <目标目录>/.tmp/。
                                   // 消费方 = compress_client 回写路径（W5）：datanode 宿主
                                   // 启动清扫 + 原子替换均发生在 client 侧（§3.1.4）；server
                                   // 不落盘，不消费本字段。
                                   // 须与目标文件同卷（st_dev 校验，跨卷 Open 即拒绝）
  // 判死参数（§3.2：阈值按心跳周期倍数设定，扫描周期远小于阈值）
  int heartbeat_interval_sec = 5;  // server 心跳周期
  int death_ttl_sec = 20;          // 判死阈值 = 心跳间隔 × 4，容忍偶发丢失与抖动
  int death_scan_interval_sec = 2; // 判死扫描周期，1~2s 取上限

  // server 执行模型（票 06 CP6 装配参数）：
  // 内存预算 ≈ queue_capacity×payload + worker_count×(payload + out≈payload×1.125)
  // 以 4MB 块计：32×4MB + 4×4.5MB ≈ 146MB；容量产生延迟而非拒绝，超发缓冲
  int server_worker_count = 0;    // 压缩 worker 线程数；0 = 按硬件并发自动
                                  // （std::thread::hardware_concurrency，下限 1）。
                                  // 压缩为 CPU 密集，与核数对齐；联调/测试显式指定固定值
  int server_queue_capacity = 32; // 有界排队缓冲；容量+线程=系统容量上限（§5.5 背压）
};

}  // namespace compress
