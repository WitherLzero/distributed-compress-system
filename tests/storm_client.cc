// 票 07 CP7.2 storm 格：单进程模拟 N 个虚拟 server 实例并发心跳。
// 用途：观测 master 在 N×(1/周期) QPS 心跳下的锁竞争（/hotspots）、RSS（/status）、
// 连接面（/connections、FD）；启动爆发模式模拟调度系统批量拉起实例的场景。
// 用法: storm_client <master_addr> [--n 300] [--duration 60] [--burst]
//   默认稳态错相（每 interval/n 毫秒唤醒一个实例，到达相位均匀摊开）；
//   --burst 取消错相，N 个实例 t=0 齐发（启动风暴），之后各自漂移进入稳态。
// 心跳周期服从 master 裁定（HeartbeatResponse.beat_interval_ms），与真实 reporter 一致。
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <bthread/bthread.h>
#include <brpc/channel.h>
#include <butil/time.h>

#include "compress_service.pb.h"

namespace {

struct Config {
  std::string addr;
  int n = 300;
  int duration_sec = 60;
  bool burst = false;
};

struct Stats {
  std::atomic<long> sent{0};
  std::atomic<long> ok{0};
  std::atomic<long> fail{0};
  std::vector<int64_t> first_latency_us;  // 每实例首跳 RPC 时延（启动风暴的直接度量）
};

struct Worker {
  const Config* cfg;
  Stats* st;
  std::atomic<bool>* stop;
  int idx;
};

void* RunInstance(void* arg) {
  auto* w = static_cast<Worker*>(arg);
  brpc::Channel ch;
  brpc::ChannelOptions opts;
  opts.timeout_ms = 2000;
  opts.max_retry = 0;
  // 关键：brpc 的 SocketMap 按 {对端地址, ChannelSignature} 全局共享 socket——
  // 默认签名全零，300 个同参数 Channel 会塌缩成 1 条 TCP（实测 /connections 仅 1 行）。
  // connection_group 参与签名哈希，每个虚拟实例给唯一值即可强制独立 socket，
  // 忠实还原 300 台真实机器的 FD/连接面形状（channel.cpp ComputeChannelSignature）。
  opts.connection_type = "single";
  opts.connection_group = "storm-" + std::to_string(w->idx);
  if (ch.Init(w->cfg->addr.c_str(), &opts) != 0) {
    w->st->fail.fetch_add(1);
    return nullptr;
  }
  compress::system::RegistryService_Stub stub(&ch);

  int64_t interval_ms = 1000;
  // 错相：把 N 个实例的首跳均匀摊到一个周期内；--burst 时全部 t=0 齐发
  const int64_t delay_ms = w->cfg->burst ? 0 : (int64_t)w->idx * interval_ms / w->cfg->n;
  if (delay_ms > 0) bthread_usleep(delay_ms * 1000);

  bool first = true;
  while (!w->stop->load(std::memory_order_relaxed)) {
    const int64_t t0 = butil::cpuwide_time_us();
    brpc::Controller cntl;
    compress::system::HeartbeatRequest req;
    compress::system::HeartbeatResponse resp;
    req.set_server_id("storm-" + std::to_string(w->idx));
    req.set_port(40000 + w->idx);
    req.set_queue_len(0);
    stub.Heartbeat(&cntl, &req, &resp, nullptr);
    const int64_t lat = butil::cpuwide_time_us() - t0;
    w->st->sent.fetch_add(1);
    if (cntl.Failed()) {
      w->st->fail.fetch_add(1);
    } else {
      w->st->ok.fetch_add(1);
      if (first) w->st->first_latency_us[w->idx] = lat;
      if (resp.beat_interval_ms() > 0) interval_ms = resp.beat_interval_ms();
    }
    first = false;
    bthread_usleep(interval_ms * 1000);
  }
  return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
  Config cfg;
  if (argc < 2) {
    std::fprintf(stderr, "usage: storm_client <master_addr> [--n N] [--duration SEC] [--burst]\n");
    return 2;
  }
  cfg.addr = argv[1];
  for (int i = 2; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--n" && i + 1 < argc) cfg.n = std::atoi(argv[++i]);
    else if (a == "--duration" && i + 1 < argc) cfg.duration_sec = std::atoi(argv[++i]);
    else if (a == "--burst") cfg.burst = true;
    else { std::fprintf(stderr, "unknown arg: %s\n", a.c_str()); return 2; }
  }

  Stats st;
  st.first_latency_us.assign(cfg.n, -1);
  std::atomic<bool> stop{false};
  std::vector<Worker> workers(cfg.n);
  std::vector<bthread_t> tids(cfg.n);
  for (int i = 0; i < cfg.n; ++i) {
    workers[i] = Worker{&cfg, &st, &stop, i};
    if (bthread_start_background(&tids[i], nullptr, RunInstance, &workers[i]) != 0) {
      std::fprintf(stderr, "bthread_start failed at %d\n", i);
      return 1;
    }
  }
  std::fprintf(stderr, "storm: n=%d duration=%ds burst=%d -> %s\n",
               cfg.n, cfg.duration_sec, cfg.burst ? 1 : 0, cfg.addr.c_str());

  for (int t = 0; t < cfg.duration_sec;) {
    const int chunk = std::min(10, cfg.duration_sec - t);
    bthread_usleep(chunk * 1000 * 1000);
    t += chunk;
    std::fprintf(stderr, "  [%3ds] sent=%ld ok=%ld fail=%ld\n",
                 t, st.sent.load(), st.ok.load(), st.fail.load());
  }
  stop.store(true);
  for (int i = 0; i < cfg.n; ++i) bthread_join(tids[i], nullptr);

  int64_t lat_max = 0, lat_sum = 0; int lat_cnt = 0;
  for (int64_t v : st.first_latency_us) {
    if (v < 0) continue;
    lat_max = std::max(lat_max, v);
    lat_sum += v; ++lat_cnt;
  }
  const double qps = cfg.duration_sec > 0 ? (double)st.ok.load() / cfg.duration_sec : 0.0;
  std::fprintf(stderr, "storm done: sent=%ld ok=%ld fail=%ld achieved_qps=%.1f\n",
               st.sent.load(), st.ok.load(), st.fail.load(), qps);
  if (lat_cnt > 0) {
    std::fprintf(stderr, "first-beat latency: avg=%ldus max=%ldus (startup burst 烈度)\n",
                 lat_sum / lat_cnt, lat_max);
  }
  return st.fail.load() > 0 ? 1 : 0;
}
