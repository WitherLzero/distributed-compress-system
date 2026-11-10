// 扫描线程 → 压缩执行线程之间的候选任务队列。
// header-only、有界、线程安全：满时拒绝（不覆盖已有任务、不无限堆积），空时阻塞等待（不忙等）。
// 纯逻辑组件：不碰 IO/网络（设计文档 §7.2）。
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace compress {

// 一个候选压缩任务：一个冷却到期的块。
// W3 骨架期只带定位信息；调度所需其余字段（冷却时长、压缩级别等）随实现期补充。
struct CompressTask {
  std::string block_path;
};

class CompressTaskQueue {
 public:
  explicit CompressTaskQueue(std::size_t capacity) : capacity_(capacity) {}

  CompressTaskQueue(const CompressTaskQueue&) = delete;
  CompressTaskQueue& operator=(const CompressTaskQueue&) = delete;

  /**
   * @brief 非阻塞入队：成功立即返回，队列满或已关闭立即拒绝（绝不阻塞、绝不覆盖已有任务）。
   * @param task 候选任务，入队成功后所有权移交给队列（内部移动构造）
   * @return true = 入队成功；false = 队列满（背压拒绝，调用方应保留任务下轮重试）
   *              或已关闭（永久拒绝）。两种失败都不触碰 task 之外的任何状态
   */
  bool TryPush(CompressTask task) {
    std::lock_guard<std::mutex> lock(mu_);
    if (closed_ || queue_.size() >= capacity_) {
      return false;
    }
    queue_.push_back(std::move(task));
    cv_.notify_one();
    return true;
  }

  /**
   * @brief 阻塞出队：空队列挂起等待，直至有任务可取或队列关闭被唤醒。
   * @param out 任务输出参数，不得为 nullptr（防呆：传 nullptr 返回 false，不做解引用）
   * @return true = 取到任务（*out 被移动赋值）；false = out 为 nullptr，
   *              或队列已关闭且存量排干（消费者线程的唯一退出信号）
   */
  bool Pop(CompressTask* out) {
    if (out == nullptr) {
      return false;
    }
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) {
      return false;
    }
    *out = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  /**
   * @brief 关闭队列（优雅关闭的入口焊死）：拒绝一切后续入队，唤醒全部等待中的消费者。
   * 关闭不清空存量：消费者继续 Pop 直至取空，此后 Pop 返回 false，线程据此退出。
   * 幂等；与「生产者全部结束 → Close → 消费者排干退出」的关闭协议配套使用。
   */
  void Close() {
    std::lock_guard<std::mutex> lock(mu_);
    closed_ = true;
    cv_.notify_all();
  }

  std::size_t Size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.size();
  }

 private:
  const std::size_t capacity_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<CompressTask> queue_;
  bool closed_ = false;
};

}  // namespace compress
