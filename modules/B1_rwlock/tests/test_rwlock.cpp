// =============================================================================
//  B1 测试文件 —— 检查你的读写锁 RWLock 写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：四个方法先退化成"独占锁"（读也串行）。于是「互斥正确性」用例本就为绿
//  （独占当然不会读写重叠、计数也准），但「多读并发」用例会红——读者无法同时持锁，
//  峰值并发恒为 1。你的任务：让多个读者并行、同时保持写者独占，把那条变绿。
//  全程不数据竞争、不死锁（独占锁是安全的），只是读不够并行。
// =============================================================================
#include "test_framework.hpp"
#include "rwlock.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

using namespace cppbc;

namespace {

struct Shared {
    RWLock            lock;
    long long         counter = 0;          // 非原子：仅写者持写锁时改，靠互斥保证不丢更新
    std::atomic<int>  active_readers{0};
    std::atomic<int>  active_writers{0};
    std::atomic<int>  max_readers{0};       // 读者峰值并发数
    std::atomic<bool> violation{false};     // 是否出现读写重叠 / 写者不独占
};

void reader_op(Shared& s, int iters) {
    for (int k = 0; k < iters; ++k) {
        RWLock::ReadGuard g(s.lock);
        int r = ++s.active_readers;
        int prev = s.max_readers.load();
        while (r > prev && !s.max_readers.compare_exchange_weak(prev, r)) { /* retry */ }
        if (s.active_writers.load() != 0) s.violation = true;   // 读时不该有写者
        long long sink = s.counter; (void)sink;                 // 读共享数据
        if (s.active_writers.load() != 0) s.violation = true;
        --s.active_readers;
    }
}

void writer_op(Shared& s, int iters) {
    for (int k = 0; k < iters; ++k) {
        RWLock::WriteGuard g(s.lock);
        int w = ++s.active_writers;
        if (w != 1) s.violation = true;                         // 写者必须独占
        if (s.active_readers.load() != 0) s.violation = true;   // 写时不该有读者
        ++s.counter;                                            // 非原子自增，靠写锁保护
        if (s.active_readers.load() != 0) s.violation = true;
        --s.active_writers;
    }
}

} // namespace

// ---- 写者互斥：并发自增非原子计数，结果必须精确（不丢更新、不重叠）----
TEST(B1_writers, exclusive_counter_exact) {
    Shared s;
    const int W = 4, ITERS = 2000;
    std::vector<std::thread> ts;
    for (int i = 0; i < W; ++i) ts.emplace_back(writer_op, std::ref(s), ITERS);
    for (auto& t : ts) t.join();

    EXPECT_FALSE(s.violation.load());
    EXPECT_EQ(s.counter, static_cast<long long>(W) * ITERS);
}

// ---- 多读并发：多个读者应能同时持锁，峰值并发 >= 2 ----
TEST(B1_readers, run_in_parallel) {
    Shared s;
    const int N = 8;
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i) {
        ts.emplace_back([&s] {
            RWLock::ReadGuard g(s.lock);
            int r = ++s.active_readers;
            int prev = s.max_readers.load();
            while (r > prev && !s.max_readers.compare_exchange_weak(prev, r)) { /* retry */ }
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 让其他读者也挤进来
            --s.active_readers;
        });
    }
    for (auto& t : ts) t.join();

    EXPECT_GE(s.max_readers.load(), 2);     // 骨架：读被串行化 → 恒为 1 → 在此变红
}

// ---- 混合读写：高竞争下不重叠、计数精确 ----
TEST(B1_mixed, readers_writers_no_overlap) {
    Shared s;
    const int W = 3, R = 5, ITERS = 1000;
    std::vector<std::thread> ts;
    for (int i = 0; i < W; ++i) ts.emplace_back(writer_op, std::ref(s), ITERS);
    for (int i = 0; i < R; ++i) ts.emplace_back(reader_op, std::ref(s), ITERS);
    for (auto& t : ts) t.join();

    EXPECT_FALSE(s.violation.load());
    EXPECT_EQ(s.counter, static_cast<long long>(W) * ITERS);
}
