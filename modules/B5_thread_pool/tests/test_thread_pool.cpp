// =============================================================================
//  B5 测试文件 —— 检查你的 thread_pool.hpp 写对了没有
// -----------------------------------------------------------------------------
//  骨架里 submit 桩"返回默认值、不真正执行任务"，worker_loop 桩是空的，
//  所以一开始测试只会失败、不会卡死。实现后才会有真正的并发执行行为。
//  若实现 bug 导致 future.get() 永远等不到结果而死锁，CTest 30s 超时会终止（见 CMakeLists）。
// =============================================================================
#include "test_framework.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <future>
#include <vector>

using namespace cppbc;

// ---- 提交带返回值的任务，future 能取回正确结果 ----
TEST(B5_basic, returns_values) {
    ThreadPool pool(4);

    auto f1 = pool.submit([] { return 42; });
    auto f2 = pool.submit([](int a, int b) { return a + b; }, 20, 22);

    // 注意：future.get() 只能调用一次（取过结果后 future 即失效）。
    // 所以先把结果存进局部变量，再交给断言——切勿写 EXPECT_EQ(f1.get(), 42)。
    int r1 = f1.get();
    int r2 = f2.get();
    EXPECT_EQ(r1, 42);
    EXPECT_EQ(r2, 42);
}

// ---- 大量任务全部被执行（无丢失） ----
TEST(B5_basic, all_tasks_run) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futs;
    for (int i = 0; i < 200; ++i) {
        futs.push_back(pool.submit([&] { counter.fetch_add(1); }));
    }
    for (auto& f : futs) f.get();   // 等全部完成

    EXPECT_EQ(counter.load(), 200);
}

// ---- 带返回值的任务求和，证明无丢失、无重复 ----
TEST(B5_results, sum_no_loss) {
    ThreadPool pool(4);

    std::vector<std::future<int>> futs;
    for (int i = 1; i <= 100; ++i) {
        futs.push_back(pool.submit([i] { return i; }));
    }
    long long sum = 0;
    for (auto& f : futs) sum += f.get();

    EXPECT_EQ(sum, 5050LL);   // 1+2+...+100
}

// ---- 优雅关闭：析构前已入队的任务必须全部执行完，不能丢 ----
TEST(B5_shutdown, drains_queued_tasks) {
    std::atomic<int> done{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 50; ++i) {
            pool.submit([&] { done.fetch_add(1); });   // 故意不保存 future、不手动等
        }
        // 离开作用域 → 触发析构 → 应"跑完队列里剩余任务再退出"。
    }
    EXPECT_EQ(done.load(), 50);
}
