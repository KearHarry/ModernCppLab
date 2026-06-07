// =============================================================================
//  A4 测试文件 —— 检查你的 Function（类型擦除）写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：转换构造体为空 → callable_ 恒为 nullptr；operator() 恒返回默认值(0/"")；
//  clone 返回 nullptr。所以一开始所有"调用得到正确结果"的断言都会变红，但因为
//  operator() 不会解引用空指针、拷贝/移动对空对象也都有空值保护，全程不崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "function.hpp"

#include <string>

using namespace cppbc;

// 一个普通自由函数（用来验证"装函数指针"）。
static int add(int a, int b) { return a + b; }

// 一个带状态的仿函数；operator() 标 const，才能被 Function 的 const 调用路径调用。
struct Multiplier {
    int k;
    int operator()(int x) const { return k * x; }
};

// ---- 核心：同一个 Function 类型，装下函数指针 / lambda / 仿函数并正确调用 ----
TEST(A4_call, stores_and_invokes_various_callables) {
    Function<int(int, int)> fp = &add;                        // 函数指针
    EXPECT_EQ(fp(2, 3), 5);

    Function<int(int, int)> lam = [](int a, int b) { return a * b; };   // 无捕获 lambda
    EXPECT_EQ(lam(4, 5), 20);

    int bias = 100;
    Function<int(int, int)> cap = [bias](int a, int b) { return bias + a + b; }; // 有捕获 lambda
    EXPECT_EQ(cap(2, 3), 105);

    Function<int(int)> fun = Multiplier{3};                   // 带状态的仿函数
    EXPECT_EQ(fun(7), 21);
}

// ---- 任意签名：零参 + 非平凡返回类型（string）----
TEST(A4_signature, zero_arg_and_string_return) {
    Function<std::string()> hello = [] { return std::string("hi"); };
    EXPECT_EQ(hello(), std::string("hi"));
}

// ---- explicit operator bool：空 vs 已绑定 ----
TEST(A4_bool, empty_vs_bound) {
    Function<int(int, int)> empty;
    EXPECT_FALSE(static_cast<bool>(empty));

    Function<int(int, int)> bound = &add;
    EXPECT_TRUE(static_cast<bool>(bound));

    Function<int(int, int)> cleared = nullptr;
    EXPECT_FALSE(static_cast<bool>(cleared));
}

// ---- 拷贝 = 虚拷贝(clone)：原件与副本各自独立、都能正确调用 ----
TEST(A4_copy, copy_clones_target) {
    Function<int(int)> f = [k = 10](int x) { return k * x; };
    Function<int(int)> g = f;                  // 拷贝构造 → 内部走 clone()
    ASSERT_TRUE(static_cast<bool>(g));         // 骨架 clone/构造为空 → 在此中止（红，不崩）
    EXPECT_EQ(f(2), 20);                       // 原件仍可用
    EXPECT_EQ(g(2), 20);                       // 副本是独立的同类型对象，结果一致
}

// ---- 移动：转移所有权，源变空、目标可用 ----
TEST(A4_move, move_transfers_ownership) {
    Function<int(int)> f = [](int x) { return x + 1; };
    Function<int(int)> g = std::move(f);
    ASSERT_TRUE(static_cast<bool>(g));
    EXPECT_EQ(g(41), 42);
}
