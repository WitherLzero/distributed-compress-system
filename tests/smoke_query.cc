// 票 07 查询 harness（Client 侧仅测试 harness，无业务逻辑——Client 模块归 W5）。
// 发 GetWorkersInfo 打印完整注册表（server_id/ip/port/queue_len/cpu/mem/last_hb），
// 既是断言工具也是观察工具。用法：
//   smoke_query <master_addr> [expected_count]
// 退出码：RPC 失败=2；带 expected_count 且数量不符=1；相符=0；不带期望数恒 0（纯观察）。
#include <cstdint>
#include <iostream>
#include <string>

#include <brpc/channel.h>

#include "compress_service.pb.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: smoke_query <master_addr> [expected_count]" << std::endl;
    return 2;
  }
  const std::string addr = argv[1];
  int expected = -1;
  if (argc >= 3) {
    expected = std::stoi(argv[2]);
  }

  brpc::Channel ch;
  brpc::ChannelOptions opts;
  opts.timeout_ms = 2000;
  opts.max_retry = 0;
  if (ch.Init(addr.c_str(), &opts) != 0) {
    std::cerr << "channel init failed: " << addr << std::endl;
    return 2;
  }

  compress::system::MasterService_Stub stub(&ch);
  brpc::Controller cntl;
  compress::system::WorkersInfoRequest req;
  compress::system::WorkersInfoResponse resp;
  stub.GetWorkersInfo(&cntl, &req, &resp, nullptr);
  if (cntl.Failed()) {
    std::cerr << "GetWorkersInfo failed: " << cntl.ErrorText() << std::endl;
    return 2;
  }

  std::cout << "workers=" << resp.workers_size() << std::endl;
  for (int i = 0; i < resp.workers_size(); ++i) {
    const auto& w = resp.workers(i);
    std::cout << "  [" << i << "] " << w.server_id() << " " << w.ip() << ":" << w.port()
              << " queue=" << w.queue_len() << " cpu=" << w.cpu_percent()
              << "% mem=" << w.mem_percent() << "% last_hb_ms=" << w.last_hb_ms() << std::endl;
  }
  if (expected >= 0 && resp.workers_size() != expected) {
    std::cerr << "ASSERT FAIL: expect " << expected << ", got " << resp.workers_size()
              << std::endl;
    return 1;
  }
  return 0;
}
