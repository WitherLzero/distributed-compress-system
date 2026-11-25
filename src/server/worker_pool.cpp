#include "worker_pool.h"

#include <google/protobuf/stubs/callback.h>

#include <cstring>
#include <string>

#include "compress_service.pb.h"
#include "compress/error_code.h"

namespace compress {

namespace {
constexpr std::size_t kLzmaSlackDiv = 8;  // 输出缓冲上界：in + in/8 + 64，覆盖两算法最坏膨胀
}  // namespace

WorkerPool::WorkerPool(std::size_t worker_count, std::size_t queue_capacity)
    : capacity_(queue_capacity) {
  workers_.reserve(worker_count);
  for (std::size_t i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this] { WorkerLoop(); });
  }
}

bool WorkerPool::TryPush(RpcCompressTaskPtr task) {
  std::lock_guard<std::mutex> lock(mu_);
  if (closed_ || queue_.size() >= capacity_) {
    return false;  // 背压拒绝：task 原样交还调用方处置（handler 走 ELIMIT 快路径）
  }
  queue_.push_back(std::move(task));
  cv_.notify_one();
  return true;
}

void WorkerPool::Stop() noexcept {
  {
    std::lock_guard<std::mutex> lock(mu_);
    closed_ = true;
  }
  cv_.notify_all();
  for (auto& w : workers_) {
    if (w.joinable()) {
      w.join();
    }
  }
  workers_.clear();
  // 残余排干：worker 数为 0（纯队列形态）或关闭后仍滞留的任务，必须补 Run——
  // 漏一个，等这条请求的 client 同步调用就永远挂在 nullptr 上（优雅退出不挂死的真身）。
  std::deque<RpcCompressTaskPtr> rest;
  {
    std::lock_guard<std::mutex> lock(mu_);
    rest.swap(queue_);
  }
  while (!rest.empty()) {
    FailTask(std::move(rest.front()), "server shutting down");
    rest.pop_front();
  }
}

void WorkerPool::WorkerLoop() {
  for (;;) {
    RpcCompressTaskPtr task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
      if (queue_.empty()) {
        return;  // 已关闭且存量排干：唯一退出出口
      }
      task = std::move(queue_.front());
      queue_.pop_front();
    }
    // —— 压缩（临界区外：慢事不进锁）——
    auto codec = CodecFactory::Create(task->level);
    if (codec == nullptr) {
      task->resp->set_result(system::FAILED);
      task->resp->set_err_msg("unknown compress level");
      task->done->Run();
      continue;
    }
    std::string in;
    task->payload.copy_to(&in);  // 连续化：ICodec 契约收裸指针，IOBuf 可能分块
    std::vector<std::uint8_t> out(in.size() + in.size() / kLzmaSlackDiv + 64);
    std::size_t out_len = 0;
    CompressErr err = codec->Compress(reinterpret_cast<const std::uint8_t*>(in.data()),
                                      in.size(), out.data(), out.size(), &out_len);
    if (err == CompressErr::kBufSmall) {  // out_len 已回报所需容量，扩容重调一次
      out.resize(out_len);
      err = codec->Compress(reinterpret_cast<const std::uint8_t*>(in.data()), in.size(),
                            out.data(), out.size(), &out_len);
    }
    if (err == CompressErr::kOk) {
      task->resp->set_result(system::OK);  // 无条件返回含膨胀结果，收益校验在 client（§5.4）
      task->resp->set_compressed_len(out_len);
      task->cntl->response_attachment().append(out.data(), out_len);
    } else {
      task->resp->set_result(system::FAILED);
      task->resp->set_err_msg(ToString(err));
    }
    task->done->Run();  // worker 线程触发：此刻框架才能序列化 resp 发回
  }
}

void WorkerPool::FailTask(RpcCompressTaskPtr task, const char* msg) noexcept {
  if (task == nullptr || task->done == nullptr) {
    return;
  }
  task->resp->set_result(system::FAILED);
  task->resp->set_err_msg(msg);
  task->done->Run();
}

std::size_t WorkerPool::PendingTasks() {
  std::lock_guard<std::mutex> lk(mu_);
  return queue_.size();
}

}  // namespace compress
