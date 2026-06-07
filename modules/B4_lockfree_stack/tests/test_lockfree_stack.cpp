// =============================================================================
//  B4 测试文件 —— 检查你的无锁栈 push/pop 写对了没有
// -----------------------------------------------------------------------------
//  并发用例只做"多线程并发 push + 单线程 drain"，因为本模块 pop 的即时 delete 在
//  并发 pop 下不安全（见头文件导读的 ABA / 回收说明）。骨架阶段 push 是空操作、
//  pop 恒返回 false，所以计数类断言会变红，但 join 立即返回、pop 不解引用，全程不崩、不卡。
// =============================================================================
#include "test_framework.hpp"
#include "lockfree_stack.hpp"

#include <thread>
#include <vector>

using namespace cppbc;

// ---- 单线程：后进先出(LIFO) + size + 空栈 pop 返回 false ----
TEST(B4_basic, lifo_order_and_size) {
    LockFreeStack<int> s;
    EXPECT_TRUE(s.empty());

    s.push(1);
    s.push(2);
    s.push(3);
    EXPECT_EQ(s.size(), static_cast<std::size_t>(3));

    int v = 0;
    ASSERT_TRUE(s.pop(v));   // 骨架 pop 恒 false → 在此中止（红，不崩）
    EXPECT_EQ(v, 3);         // 栈顶最先出来
    ASSERT_TRUE(s.pop(v));
    EXPECT_EQ(v, 2);
    ASSERT_TRUE(s.pop(v));
    EXPECT_EQ(v, 1);

    EXPECT_FALSE(s.pop(v));  // 空栈：返回 false
    EXPECT_TRUE(s.empty());
}

// ---- 并发 push：多线程同时入栈，CAS 重试不能丢节点 ----
TEST(B4_concurrent, concurrent_push_then_drain) {
    constexpr int kThreads = 4;
    constexpr int kPerThread = 1000;

    LockFreeStack<int> s;
    std::vector<std::thread> workers;
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&s] {
            for (int i = 1; i <= kPerThread; ++i) s.push(i);
        });
    }
    for (auto& w : workers) w.join();

    // 全部 push 完成后，单线程逐个 drain，统计数量与总和验证无丢失/无重复。
    long long sum = 0;
    int count = 0;
    int v = 0;
    while (s.pop(v)) {
        sum += v;
        ++count;
    }

    EXPECT_EQ(count, kThreads * kPerThread);
    // 每个线程都 push 了 1..kPerThread，总和 = 线程数 × (1+2+...+kPerThread)。
    const long long per = static_cast<long long>(kPerThread) * (kPerThread + 1) / 2;
    EXPECT_EQ(sum, static_cast<long long>(kThreads) * per);
    EXPECT_TRUE(s.empty());
}
