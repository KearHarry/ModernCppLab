// =============================================================================
//  B3 测试文件 —— 检查你的 atomics.hpp 写对了没有
// -----------------------------------------------------------------------------
//  并发小贴士：骨架里 try_lock 桩"假装总能抢到锁"、fetch_max 桩"只读不更新"，
//  所以一开始测试只会失败、不会卡死。实现后才会出现真正的互斥/原子更新行为。
//  若实现有 bug 导致死锁，CTest 设了 30s 超时会自动终止（见本模块 CMakeLists）。
// =============================================================================
#include "test_framework.hpp"
#include "atomics.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace cppbc;

// ---- 单线程：try_lock 的基本语义 ----
TEST(B3_spinlock, try_lock_basic) {
    SpinLock lk;

    EXPECT_TRUE(lk.try_lock());    // 没人占用 → 抢到
    EXPECT_FALSE(lk.try_lock());   // 已被自己占用 → 第二次抢不到
    lk.unlock();                   // 释放
    EXPECT_TRUE(lk.try_lock());    // 释放后又能抢到
    lk.unlock();
}

// ---- 多线程：自旋锁保护的累加，结果必须精确 ----
//  8 个线程各自 +20000 次；若锁正确，总数恰为 160000（无丢失更新）。
TEST(B3_spinlock, protects_counter) {
    constexpr int kThreads = 8;
    constexpr int kPerThread = 20000;

    SpinLock lk;
    long counter = 0;   // 故意用普通 long：靠自旋锁保护，而非原子变量

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&] {
            for (int i = 0; i < kPerThread; ++i) {
                lk.lock();
                ++counter;        // 临界区：同一时刻只能有一个线程在这里
                lk.unlock();
            }
        });
    }
    for (auto& t : ts) t.join();

    EXPECT_EQ(counter, static_cast<long>(kThreads) * kPerThread);  // 160000
}

// ---- 单线程：atomic_fetch_max 的返回值与更新效果 ----
TEST(B3_fetch_max, single_thread) {
    std::atomic<long> v{10};

    // 用更大的值更新：返回旧值 10，且 v 变成 20
    EXPECT_EQ(atomic_fetch_max(v, 20), 10L);
    EXPECT_EQ(v.load(), 20L);

    // 用更小的值更新：不改变，返回当前值 20
    EXPECT_EQ(atomic_fetch_max(v, 5), 20L);
    EXPECT_EQ(v.load(), 20L);

    // 用相等的值更新：不改变，返回当前值 20
    EXPECT_EQ(atomic_fetch_max(v, 20), 20L);
    EXPECT_EQ(v.load(), 20L);
}

// ---- 多线程：并发求最大值，最终一定等于所有线程写入的最大值 ----
//  每个线程尝试把 v 抬高到自己的目标值；CAS 循环保证不会"丢失"最大值。
TEST(B3_fetch_max, concurrent_max) {
    constexpr int kThreads = 16;
    std::atomic<long> v{0};

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        // 目标值：100, 200, ..., 1600；最大应为 1600
        long target = static_cast<long>(t + 1) * 100;
        ts.emplace_back([&v, target] {
            // 多抬几次，制造更多并发竞争
            for (int i = 0; i < 1000; ++i) atomic_fetch_max(v, target);
        });
    }
    for (auto& t : ts) t.join();

    EXPECT_EQ(v.load(), static_cast<long>(kThreads) * 100);  // 1600
}
