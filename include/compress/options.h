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
                                   // 须与目标文件同卷（st_dev 校验，跨卷 Open 即拒绝）
};

}  // namespace compress
