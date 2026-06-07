// =============================================================================
//  D3 测试文件 —— 检查你的 singleton.hpp 写对了没有
// -----------------------------------------------------------------------------
//  核心检验：让 16 个线程同时第一次访问单例，必须满足
//    (1) 所有线程拿到的是【同一个对象】（指针全相等）；
//    (2) 构造函数【恰好执行一次】（ctor_count == 1）。
//  骨架阶段 instance() 每次都 new 一个新对象 → 指针各不相同、ctor_count 远大于 1，
//  于是变红；但每个指针都有效、不会崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "singleton.hpp"

#include <thread>
#include <vector>

using namespace cppbc;

// ---- ① Meyers 单例：并发首次访问下唯一实例 + 构造一次 + 状态共享 ----
TEST(D3_meyers, one_instance_under_contention) {
    constexpr int N = 16;
    std::vector<MeyersSingleton*> ptrs(N, nullptr);
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i)
        ts.emplace_back([&ptrs, i] { ptrs[i] = &MeyersSingleton::instance(); });
    for (auto& t : ts) t.join();

    bool all_same = (ptrs[0] != nullptr);
    for (int i = 1; i < N; ++i)
        if (ptrs[i] != ptrs[0]) all_same = false;
    EXPECT_TRUE(all_same);                                 // 所有线程拿到同一对象
    EXPECT_EQ(MeyersSingleton::ctor_count.load(), 1);      // 构造恰好一次

    MeyersSingleton::instance().set(7);
    EXPECT_EQ(MeyersSingleton::instance().get(), 7);       // 同一对象 → 状态共享
}

// ---- ② call_once 单例：并发首次访问下唯一实例 + 构造一次 ----
TEST(D3_call_once, one_instance_under_contention) {
    constexpr int N = 16;
    std::vector<CallOnceSingleton*> ptrs(N, nullptr);
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i)
        ts.emplace_back([&ptrs, i] { ptrs[i] = &CallOnceSingleton::instance(); });
    for (auto& t : ts) t.join();

    bool all_same = (ptrs[0] != nullptr);
    for (int i = 1; i < N; ++i)
        if (ptrs[i] != ptrs[0]) all_same = false;
    EXPECT_TRUE(all_same);
    EXPECT_EQ(CallOnceSingleton::ctor_count.load(), 1);
}

// ---- ③ DCLP 单例：并发首次访问下唯一实例 + 构造一次 ----
TEST(D3_dclp, one_instance_under_contention) {
    constexpr int N = 16;
    std::vector<DclpSingleton*> ptrs(N, nullptr);
    std::vector<std::thread> ts;
    for (int i = 0; i < N; ++i)
        ts.emplace_back([&ptrs, i] { ptrs[i] = &DclpSingleton::instance(); });
    for (auto& t : ts) t.join();

    bool all_same = (ptrs[0] != nullptr);
    for (int i = 1; i < N; ++i)
        if (ptrs[i] != ptrs[0]) all_same = false;
    EXPECT_TRUE(all_same);
    EXPECT_EQ(DclpSingleton::ctor_count.load(), 1);
}
