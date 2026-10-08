// =============================================================================
//  B6 测试：生命周期、jthread 停止、可取消等待、scoped_lock
//  骨架的异步操作均立即返回，因此开箱只红测、不死锁。
// =============================================================================
#include "test_framework.hpp"
#include "thread_lifecycle.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace cppbc;

TEST(B6_joining_thread, destructor_joins_started_work) {
    std::atomic<int> result{0};
    {
        JoiningThread worker([&] { result.store(42, std::memory_order_relaxed); });
        EXPECT_TRUE(worker.joinable()); // 骨架没有启动线程，在这里变红。
    }
    EXPECT_EQ(result.load(std::memory_order_relaxed), 42);
}

TEST(B6_joining_thread, explicit_join_waits_exactly_once) {
    std::atomic<int> calls{0};
    JoiningThread worker([&] { calls.fetch_add(1, std::memory_order_relaxed); });
    worker.join();
    worker.join(); // 幂等：第二次不应对已 join 的 thread 再 join。
    EXPECT_FALSE(worker.joinable());
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 1);
}

TEST(B6_jthread, stop_token_reaches_cooperative_task) {
    std::atomic<int> ticks{0};
    std::atomic<bool> finished{false};
    std::jthread worker([&](std::stop_token st) {
        CooperativeCounter{}(st, ticks);
        finished.store(true, std::memory_order_release);
    });

    // 等到任务确实做出一次进展，或安全骨架已经返回；不靠固定 sleep 猜调度时机。
    while (ticks.load(std::memory_order_relaxed) == 0 &&
           !finished.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    worker.request_stop();
    worker.join();

    EXPECT_GT(ticks.load(std::memory_order_relaxed), 0); // 骨架立即返回 → 红。
    EXPECT_TRUE(finished.load(std::memory_order_acquire));
}

TEST(B6_stoppable_queue, consumes_value_already_available) {
    StoppableQueue<int> q;
    q.push(7);
    std::stop_source source;
    int out = -1;
    EXPECT_TRUE(q.wait_pop(source.get_token(), out)); // 骨架 false → 红。
    EXPECT_EQ(out, 7);
    EXPECT_EQ(q.size(), std::size_t{0});
}

TEST(B6_stoppable_queue, stop_cancels_empty_wait) {
    StoppableQueue<int> q;
    std::atomic<bool> returned{false};
    std::atomic<bool> popped{true};

    std::jthread consumer([&](std::stop_token st) {
        int out = 0;
        popped.store(q.wait_pop(st, out), std::memory_order_relaxed);
        returned.store(true, std::memory_order_release);
    });
    // 无论消费者已进入 wait，还是稍后才观察到 token，停止都必须可靠生效。
    consumer.request_stop();
    consumer.join();

    EXPECT_TRUE(returned.load(std::memory_order_acquire));
    EXPECT_FALSE(popped.load(std::memory_order_relaxed));
}

TEST(B6_transfer, basic_transfer_and_rejection) {
    Account a(100), b(20);
    EXPECT_TRUE(transfer(a, b, 30)); // 骨架 false → 红。
    EXPECT_EQ(a.balance, 70);
    EXPECT_EQ(b.balance, 50);
    EXPECT_FALSE(transfer(a, b, 1000));
    EXPECT_EQ(total_balance(a, b), 120);
}

TEST(B6_transfer, opposite_directions_finish_without_deadlock) {
    Account a(1000), b(1000);
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < 250; ++i) (void)transfer(a, b, 1);
        });
    }
    for (int t = 0; t < 2; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < 250; ++i) (void)transfer(b, a, 1);
        });
    }
    for (auto& worker : workers) worker.join();

    EXPECT_EQ(total_balance(a, b), 2000); // 无论实现与否，总额都不能凭空变化。
    EXPECT_EQ(a.balance, 500);            // 骨架不转账，确定性变红。
    EXPECT_EQ(b.balance, 1500);
}
