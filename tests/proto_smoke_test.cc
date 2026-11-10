// 票 05 冒烟：三对消息族「构造 -> 序列化 -> 反序列化 -> 字段一致」全链验证。
// 覆盖设计文档 §6.1.1 / 6.1.2 / 6.1.3。
#include <gtest/gtest.h>

#include "compress_service.pb.h"

namespace compress::system {
namespace {

TEST(ProtoSmoke, WorkersInfoRoundTrip) {
  WorkersInfoResponse out;
  out.mutable_header()->set_version(1);
  out.mutable_header()->set_trace_id("trace-w3-smoke");
  WorkerInfo* w = out.add_workers();
  w->set_server_id("dn-01");
  w->set_ip("10.0.0.1");
  w->set_port(8002);
  w->set_cpu_percent(12);
  w->set_mem_percent(34);
  w->set_queue_len(0);
  w->set_last_hb_ms(1699000000000LL);

  std::string bytes;
  ASSERT_TRUE(out.SerializeToString(&bytes));

  WorkersInfoResponse in;
  ASSERT_TRUE(in.ParseFromString(bytes));
  EXPECT_EQ(in.header().version(), 1u);
  EXPECT_EQ(in.header().trace_id(), "trace-w3-smoke");
  ASSERT_EQ(in.workers_size(), 1);
  EXPECT_EQ(in.workers(0).server_id(), "dn-01");
  EXPECT_EQ(in.workers(0).ip(), "10.0.0.1");
  EXPECT_EQ(in.workers(0).port(), 8002);
  EXPECT_EQ(in.workers(0).cpu_percent(), 12u);
  EXPECT_EQ(in.workers(0).mem_percent(), 34u);
  EXPECT_EQ(in.workers(0).queue_len(), 0u);
  EXPECT_EQ(in.workers(0).last_hb_ms(), 1699000000000LL);
}

TEST(ProtoSmoke, HeartbeatRoundTrip) {
  HeartbeatRequest req;
  req.mutable_header()->set_version(1);
  req.set_server_id("dn-01");
  req.set_port(8002);
  req.set_cpu_percent(56);
  req.set_mem_percent(78);
  req.set_queue_len(3);

  HeartbeatResponse resp;
  resp.mutable_header()->set_version(1);
  resp.set_accepted(true);
  resp.set_beat_interval_ms(5000);

  std::string req_bytes, resp_bytes;
  ASSERT_TRUE(req.SerializeToString(&req_bytes));
  ASSERT_TRUE(resp.SerializeToString(&resp_bytes));

  HeartbeatRequest req_in;
  HeartbeatResponse resp_in;
  ASSERT_TRUE(req_in.ParseFromString(req_bytes));
  ASSERT_TRUE(resp_in.ParseFromString(resp_bytes));
  EXPECT_EQ(req_in.server_id(), "dn-01");
  EXPECT_EQ(req_in.port(), 8002);
  EXPECT_EQ(req_in.cpu_percent(), 56u);
  EXPECT_TRUE(resp_in.accepted());
  EXPECT_EQ(resp_in.beat_interval_ms(), 5000);
}

TEST(ProtoSmoke, CompressRoundTrip) {
  // 决策记录：响应无拒绝类字段，server 无条件返回结果（issue 02 / §5.4）
  CompressRequest req;
  req.mutable_header()->set_version(1);
  req.set_data_id("blk_101");
  req.set_origin_len(4 * 1024 * 1024);
  req.set_level(DEEP);
  req.set_version(7);

  CompressResponse resp;
  resp.mutable_header()->set_version(1);
  resp.set_result(OK);
  resp.set_compressed_len(1024 * 1024);

  std::string req_bytes, resp_bytes;
  ASSERT_TRUE(req.SerializeToString(&req_bytes));
  ASSERT_TRUE(resp.SerializeToString(&resp_bytes));

  CompressRequest req_in;
  CompressResponse resp_in;
  ASSERT_TRUE(req_in.ParseFromString(req_bytes));
  ASSERT_TRUE(resp_in.ParseFromString(resp_bytes));
  EXPECT_EQ(req_in.data_id(), "blk_101");
  EXPECT_EQ(req_in.origin_len(), 4u * 1024 * 1024);
  EXPECT_EQ(req_in.level(), DEEP);
  EXPECT_EQ(req_in.version(), 7u);
  EXPECT_EQ(resp_in.result(), OK);
  EXPECT_EQ(resp_in.compressed_len(), 1024u * 1024);
  EXPECT_TRUE(resp_in.err_msg().empty());
}

}  // namespace
}  // namespace compress::system
