// =============================================================================
//  A6 测试文件 —— 检查你的 Generator<T> 写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：get_return_object 返回空盒子（无 handle）、yield_value 不存值、
//  unhandled_exception 不抓异常。于是 Generator 遍历出来恒为空，取值前都用 ASSERT_*
//  挡住，所以一开始只变红、不崩溃、也不会死循环（空 handle 的迭代器立刻等于末尾）。
//  迭代器、begin/end、移动/析构已给好。
// =============================================================================
#include "test_framework.hpp"
#include "generator.hpp"

#include <vector>
#include <stdexcept>

using namespace cppbc;

// ---- 几个协程函数：函数体里出现 co_yield，编译器即把它变成协程 ----
static Generator<int> ints123() {
    co_yield 1;
    co_yield 2;
    co_yield 3;
}

static Generator<int> range_gen(int lo, int hi) {
    for (int i = lo; i < hi; ++i) co_yield i;
}

static Generator<int> naturals() {       // 无限自然数 0,1,2,...
    int i = 0;
    for (;;) co_yield i++;
}

static Generator<int> fib() {            // 无限斐波那契 0,1,1,2,3,5,...
    long long a = 0, b = 1;
    for (;;) {
        co_yield static_cast<int>(a);
        long long n = a + b;
        a = b;
        b = n;
    }
}

static Generator<int> throwing() {       // 吐两个值后抛异常
    co_yield 1;
    co_yield 2;
    throw std::runtime_error("boom");
}

// 把一个 Generator 全部收集进 vector（按值取入：Generator 只可移动）。
static std::vector<int> collect(Generator<int> g) {
    std::vector<int> v;
    for (int x : g) v.push_back(x);
    return v;
}

// ---- 基本：按顺序 yield ----
TEST(A6_basic, yields_in_order) {
    auto v = collect(ints123());
    ASSERT_EQ(v.size(), 3u);                 // 骨架：序列为空 → 在此中止
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

// ---- 半开区间 [lo,hi) 与空区间 ----
TEST(A6_range, half_open_and_empty) {
    auto v = collect(range_gen(5, 10));
    ASSERT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 5);
    EXPECT_EQ(v[4], 9);

    auto e = collect(range_gen(3, 3));       // 空区间 → 不 yield
    EXPECT_TRUE(e.empty());
}

// ---- 惰性：无限序列只取前 5 个，不应卡死 ----
TEST(A6_lazy, infinite_taken_finitely) {
    auto g = naturals();
    std::vector<int> v;
    int n = 0;
    for (int x : g) {
        v.push_back(x);
        if (++n == 5) break;                 // 按需取值，取够就停
    }
    ASSERT_EQ(v.size(), 5u);
    EXPECT_EQ(v[0], 0);
    EXPECT_EQ(v[4], 4);
}

// ---- 斐波那契前 10 个 ----
TEST(A6_fib, first_ten) {
    auto g = fib();
    std::vector<int> v;
    int n = 0;
    for (int x : g) {
        v.push_back(x);
        if (++n == 10) break;
    }
    ASSERT_EQ(v.size(), 10u);
    const int expect[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    for (int i = 0; i < 10; ++i) EXPECT_EQ(v[i], expect[i]);
}

// ---- 协程体内的异常会传到调用方 ----
TEST(A6_exception, body_exception_propagates) {
    EXPECT_THROW(collect(throwing()), std::runtime_error);
}

// ---- 只可移动：移动后源被掏空，目标仍可用 ----
TEST(A6_move, move_only_transfers_ownership) {
    auto g = ints123();
    ASSERT_TRUE(g.valid());                  // 骨架：get_return_object 给空盒子 → 在此中止
    auto g2 = std::move(g);
    EXPECT_FALSE(g.valid());                 // 源被掏空
    auto v = collect(std::move(g2));
    ASSERT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
}
