// 票 07：CompressTaskQueue 单测。
// 契约：有界（满则拒、不覆盖）、空则阻塞、关闭语义、多生产者单消费者无丢失无重复。
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "compress/compress_task_queue.h"

using compress::CompressTask;
using compress::CompressTaskQueue;

TEST(TaskQueueTest, PushPopFifo) {
  CompressTaskQueue q(8);
  ASSERT_TRUE(q.TryPush({"blk_a"}));
  ASSERT_TRUE(q.TryPush({"blk_b"}));

  CompressTask t;
  ASSERT_TRUE(q.Pop(&t));
  EXPECT_EQ(t.block_path, "blk_a");
  ASSERT_TRUE(q.Pop(&t));
  EXPECT_EQ(t.block_path, "blk_b");
}

TEST(TaskQueueTest, BoundedRejectsWhenFull) {
  CompressTaskQueue q(4);
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(q.TryPush({"blk_" + std::to_string(i)}));
  }
  // 第 5 次入队必须被拒：不溢出、不覆盖。
  EXPECT_FALSE(q.TryPush({"blk_overflow"}));
  EXPECT_EQ(q.Size(), 4u);

  // 掏一个空位后又能入队——拒绝针对的是「满」而非「曾经满过」。
  CompressTask t;
  ASSERT_TRUE(q.Pop(&t));
  EXPECT_TRUE(q.TryPush({"blk_ok"}));
}

TEST(TaskQueueTest, CloseDrainsThenRejects) {
  CompressTaskQueue q(4);
  q.TryPush({"blk_a"});
  q.Close();

  EXPECT_FALSE(q.TryPush({"blk_late"}));  // 关闭后入队恒拒

  CompressTask t;
  ASSERT_TRUE(q.Pop(&t));  // 残余任务仍可取出
  EXPECT_EQ(t.block_path, "blk_a");
  EXPECT_FALSE(q.Pop(&t));  // 掏干后 Pop 返回 false：消费者退出信号
}

TEST(TaskQueueTest, PopRejectsNull) {
  CompressTaskQueue q(2);
  EXPECT_FALSE(q.Pop(nullptr));  // 空队列 + 空指针：不崩、返 false

  ASSERT_TRUE(q.TryPush({"blk_a"}));
  EXPECT_FALSE(q.Pop(nullptr));  // 非空队列 + 空指针：防呆生效，不做解引用
  EXPECT_EQ(q.Size(), 1u);       // 任务原样保留

  CompressTask t;
  ASSERT_TRUE(q.Pop(&t));  // 正常出队不受影响
  EXPECT_EQ(t.block_path, "blk_a");
}

TEST(TaskQueueTest, CrossThreadHandoff) {
  CompressTaskQueue q(1);
  // 子线程只记录事实，断言归主线程：gtest 的 ASSERT 在 lambda 里只会结束该 lambda，
  // 杀不掉整个测试。
  std::atomic<bool> popped_ok{false};
  std::thread consumer([&q, &popped_ok] {
    CompressTask t;
    if (q.Pop(&t) && t.block_path == "from_main") {
      popped_ok.store(true);
    }
  });
  // 队列初始为空：上面那次 Pop 必然走了条件变量等待路径。
  ASSERT_TRUE(q.TryPush({"from_main"}));
  consumer.join();
  EXPECT_TRUE(popped_ok.load());
}

TEST(TaskQueueTest, MpscNoLossNoDup) {
  constexpr int kProducers = 4;
  constexpr int kPerProducer = 500;
  CompressTaskQueue q(10000);  // 容量大于总量，正常路径不触发拒绝

  // 教科书生命周期：生产者全部 join → Close 焊死入口 → 消费者排干至 Pop 返 false 退出。
  // 这一个用例同时验证：并发正确性（无丢无重）+ 关闭协议（入口焊死 + 排干语义）。
  std::vector<std::string> popped;
  std::mutex mu;
  std::atomic<bool> push_failed{false};
  std::atomic<bool> late_push_accepted{false};

  std::thread consumer([&] {
    CompressTask t;
    while (q.Pop(&t)) {
      std::lock_guard<std::mutex> lock(mu);
      popped.push_back(t.block_path);
    }
    // 走到这里 = 已关闭且排干。此刻再推必须被拒，记事实交主线程断言。
    if (q.TryPush({"after_close"})) {
      late_push_accepted.store(true);
    }
  });

  std::vector<std::thread> producers;
  for (int p = 0; p < kProducers; ++p) {
    producers.emplace_back([p, &q, &push_failed] {
      for (int i = 0; i < kPerProducer; ++i) {
        if (!q.TryPush({"p" + std::to_string(p) + "_" + std::to_string(i)})) {
          push_failed.store(true);  // 容量足够，不应发生；记录事实交由主线程诊断
        }
      }
    });
  }
  for (auto& th : producers) {
    th.join();
  }
  q.Close();
  consumer.join();

  ASSERT_FALSE(push_failed.load());
  EXPECT_FALSE(late_push_accepted.load());  // Close 后入队恒拒

  // 期望全量同样排序后整体比对：排序是字典序，与生产顺序无关（数量相等 + 排序相等 ⇔
  // 无丢失无重复）。
  ASSERT_EQ(popped.size(), static_cast<size_t>(kProducers * kPerProducer));
  std::vector<std::string> expected;
  for (int p = 0; p < kProducers; ++p) {
    for (int i = 0; i < kPerProducer; ++i) {
      expected.push_back("p" + std::to_string(p) + "_" + std::to_string(i));
    }
  }
  std::sort(popped.begin(), popped.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(popped, expected);
}
