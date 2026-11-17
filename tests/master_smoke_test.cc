// master_smoke_test.cc —— CP0：BRPC 通信链路冒烟。
// 同一个进程内起 brpc::Server + brpc::Channel，验证 RegistryService.Heartbeat 收发。
// 导师标准（第4课转录 L562/L598）：先把通信搞通，再往里塞实现。

#include <brpc/channel.h>
#include <brpc/server.h>
#include <gtest/gtest.h>

#include "compress_service.pb.h"

// 最薄实现：Heartbeat 固定返回 accepted=true，GetWorkersInfo 返回空表。
// CP0 只验证通信链路，不验证业务逻辑。
namespace {

class SmokeRegistryService : public compress::system::RegistryService {
 public:
  void Heartbeat(google::protobuf::RpcController* /*cntl*/,
                 const compress::system::HeartbeatRequest* /*req*/,
                 compress::system::HeartbeatResponse* resp,
                 google::protobuf::Closure* done) override {
    brpc::ClosureGuard done_guard(done);
    resp->set_accepted(true);
  }
};

class SmokeMasterService : public compress::system::MasterService {
 public:
  void GetWorkersInfo(google::protobuf::RpcController* /*cntl*/,
                      const compress::system::WorkersInfoRequest* /*req*/,
                      compress::system::WorkersInfoResponse* /*resp*/,
                      google::protobuf::Closure* done) override {
    brpc::ClosureGuard done_guard(done);
    // 空表：CP0 不注册任何 worker
  }
};

}  // namespace

TEST(MasterSmoke, HeartbeatAccepted) {
  brpc::Server server;
  SmokeRegistryService registry_svc;
  SmokeMasterService master_svc;

  ASSERT_EQ(server.AddService(&registry_svc, brpc::SERVER_DOESNT_OWN_SERVICE), 0);
  ASSERT_EQ(server.AddService(&master_svc, brpc::SERVER_DOESNT_OWN_SERVICE), 0);

  brpc::ServerOptions opts;
  opts.idle_timeout_sec = -1;
  ASSERT_EQ(server.Start(0, &opts), 0);  // port 0 = 内核分配

  const int port = server.listen_address().port;
  ASSERT_GT(port, 0);

  brpc::Channel channel;
  brpc::ChannelOptions chan_opts;
  chan_opts.timeout_ms = 2000;
  const std::string addr = "127.0.0.1:" + std::to_string(port);
  ASSERT_EQ(channel.Init(addr.c_str(), &chan_opts), 0);

  compress::system::RegistryService_Stub stub(&channel);
  brpc::Controller cntl;
  compress::system::HeartbeatRequest req;
  compress::system::HeartbeatResponse resp;
  req.set_server_id("smoke-server");
  req.set_port(8002);
  stub.Heartbeat(&cntl, &req, &resp, nullptr);

  ASSERT_FALSE(cntl.Failed()) << cntl.ErrorText();
  EXPECT_TRUE(resp.accepted());

  server.Stop(0);
  server.Join();
}

TEST(MasterSmoke, GetWorkersInfoEmpty) {
  brpc::Server server;
  SmokeRegistryService registry_svc;
  SmokeMasterService master_svc;

  ASSERT_EQ(server.AddService(&registry_svc, brpc::SERVER_DOESNT_OWN_SERVICE), 0);
  ASSERT_EQ(server.AddService(&master_svc, brpc::SERVER_DOESNT_OWN_SERVICE), 0);

  brpc::ServerOptions opts;
  opts.idle_timeout_sec = -1;
  ASSERT_EQ(server.Start(0, &opts), 0);

  const int port = server.listen_address().port;
  ASSERT_GT(port, 0);

  brpc::Channel channel;
  brpc::ChannelOptions chan_opts;
  chan_opts.timeout_ms = 2000;
  const std::string addr = "127.0.0.1:" + std::to_string(port);
  ASSERT_EQ(channel.Init(addr.c_str(), &chan_opts), 0);

  compress::system::MasterService_Stub stub(&channel);
  brpc::Controller cntl;
  compress::system::WorkersInfoRequest req;
  compress::system::WorkersInfoResponse resp;
  stub.GetWorkersInfo(&cntl, &req, &resp, nullptr);

  ASSERT_FALSE(cntl.Failed()) << cntl.ErrorText();
  EXPECT_EQ(resp.workers_size(), 0);

  server.Stop(0);
  server.Join();
}
