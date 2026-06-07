// =============================================================================
//  B2 测试文件 —— 检查你的 blocking_queue.hpp 写对了没有
// -----------------------------------------------------------------------------
//  并发测试小贴士：骨架里的桩代码"立即返回不阻塞"，所以一开始只会失败、不会卡死。
//  实现后若出现死锁，CTest 设了 30s 超时会自动终止（见本模块 CMakeLists）。
// =============================================================================
#include "test_framework.hpp"
#include "blocking_queue.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace cppbc;
using namespace std::chrono_literals;

// ---- 单线程：FIFO 先进先出顺序 ----
TEST(B2_basic, fifo_order) {
    BlockingQueue<int> q(10);
    q.push(1);
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.size(), std::size_t{3});

    int x = -1;
    ASSERT_TRUE(q.pop(x)); EXPECT_EQ(x, 1);
    ASSERT_TRUE(q.pop(x)); EXPECT_EQ(x, 2);
    ASSERT_TRUE(q.pop(x)); EXPECT_EQ(x, 3);
    EXPECT_EQ(q.size(), std::size_t{0});
}

// ---- 关闭后仍能取完残留数据，取完才返回 false（不丢数据） ----
TEST(B2_close, drain_remaining_after_close) {
    BlockingQueue<int> q(4);
    q.push(10);
    q.push(20);
    q.close();

    int x = -1;
    ASSERT_TRUE(q.pop(x)); EXPECT_EQ(x, 10);   // 关闭后残留数据照取
    ASSERT_TRUE(q.pop(x)); EXPECT_EQ(x, 20);
    EXPECT_FALSE(q.pop(x));                     // 已空且已关闭 → false
}

// ---- 多生产者 / 多消费者：总条数与总和都要对上（无丢失、无重复） ----
TEST(B2_concurrency, multi_producer_consumer) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 3;
    constexpr int kPerProducer = 500;

    BlockingQueue<int> q(4);                   // 故意用小容量，逼出"满则阻塞"
    std::atomic<long long> total_sum{0};
    std::atomic<int> total_count{0};

    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            int x;
            while (q.pop(x)) {                 // 取到 false 表示队列已关闭且取空
                total_sum += x;
                total_count += 1;
            }
        });
    }

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&] {
            for (int i = 1; i <= kPerProducer; ++i) q.push(i);
        });
    }

    for (auto& t : producers) t.join();
    q.close();                                 // 生产完毕，关闭以唤醒消费者退出
    for (auto& t : consumers) t.join();

    // 每个生产者push 1..kPerProducer，期望总条数与总和：
    constexpr int expected_count = kProducers * kPerProducer;
    constexpr long long one_producer_sum =
        static_cast<long long>(kPerProducer) * (kPerProducer + 1) / 2;
    EXPECT_EQ(total_count.load(), expected_count);
    EXPECT_EQ(total_sum.load(), kProducers * one_producer_sum);
}

// ---- close() 必须能唤醒"正阻塞在空队列上的消费者"（否则死锁、join 不掉） ----
TEST(B2_close, close_wakes_blocked_consumer) {
    BlockingQueue<int> q(4);
    std::atomic<bool> pop_result{true};

    std::thread consumer([&] {
        int x;
        bool r = q.pop(x);      // 正确实现里这里会阻塞，直到 close() 唤醒
        pop_result = r;
    });

    std::this_thread::sleep_for(50ms);  // 让消费者先进入等待
    q.close();                          // 唤醒它
    consumer.join();                    // 不应卡死

    EXPECT_FALSE(pop_result.load());    // 被 close 唤醒、队列空 → 返回 false
}
