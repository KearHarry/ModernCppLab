// =============================================================================
//  C1 测试文件 —— 检查你的 vector.hpp 写对了没有
// -----------------------------------------------------------------------------
//  设计说明：凡是要读取元素值的用例，都先用 ASSERT_EQ 检查 size()。
//  这样在"骨架阶段"（push_back 还是空操作、size 为 0）ASSERT 会先失败并中止该用例，
//  绝不会去解引用空指针 → 一开始只会变红，不会崩溃。实现后 size 正确，断言才会继续。
// =============================================================================
#include "test_framework.hpp"
#include "vector.hpp"

#include <cstddef>
#include <utility>

using namespace cppbc;

// 辅助类型：每构造 +1、每析构 -1，用来验证"构造/析构是否配对"（有没有内存泄漏）。
struct Counted {
    static int alive;
    int v;
    Counted() : v(0) { ++alive; }
    explicit Counted(int x) : v(x) { ++alive; }
    Counted(const Counted& o) : v(o.v) { ++alive; }
    Counted(Counted&& o) noexcept : v(o.v) { ++alive; o.v = -1; }
    Counted& operator=(const Counted& o) { v = o.v; return *this; }
    Counted& operator=(Counted&& o) noexcept { v = o.v; o.v = -1; return *this; }
    ~Counted() { --alive; }
};
int Counted::alive = 0;

// ---- 空 vector 的初始状态 ----
TEST(C1_basic, empty_vector) {
    Vector<int> v;
    EXPECT_EQ(v.size(), std::size_t{0});
    EXPECT_EQ(v.capacity(), std::size_t{0});
    EXPECT_TRUE(v.empty());
}

// ---- push_back 后能正确按下标读取 ----
TEST(C1_push, push_back_and_index) {
    Vector<int> v;
    for (int i = 0; i < 5; ++i) v.push_back(i * 10);

    ASSERT_EQ(v.size(), std::size_t{5});      // 骨架阶段在此变红并中止，安全
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(v[0], 0);
    EXPECT_EQ(v[4], 40);
    EXPECT_GE(v.capacity(), std::size_t{5});  // 容量至少容得下
}

// ---- 容量按翻倍策略增长（push 17 个 → 容量 32） ----
TEST(C1_growth, capacity_doubles) {
    Vector<int> v;
    EXPECT_EQ(v.capacity(), std::size_t{0});

    for (int i = 0; i < 17; ++i) v.push_back(i);

    EXPECT_EQ(v.size(), std::size_t{17});
    EXPECT_GE(v.capacity(), std::size_t{17});
    EXPECT_EQ(v.capacity(), std::size_t{32});  // 1→2→4→8→16→32
}

// ---- reserve 预留容量后，后续 push 不再触发重新分配（数据指针不变） ----
TEST(C1_reserve, reserve_then_no_realloc) {
    Vector<int> v;
    v.reserve(100);
    EXPECT_EQ(v.capacity(), std::size_t{100});
    EXPECT_EQ(v.size(), std::size_t{0});

    const int* before = v.data();
    for (int i = 0; i < 100; ++i) v.push_back(i);

    EXPECT_EQ(v.size(), std::size_t{100});
    EXPECT_EQ(v.capacity(), std::size_t{100});  // 没有再扩容
    EXPECT_EQ(v.data(), before);                // 数据没搬家
}

// ---- pop_back ----
TEST(C1_pop, pop_back) {
    Vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    ASSERT_EQ(v.size(), std::size_t{3});
    v.pop_back();
    EXPECT_EQ(v.size(), std::size_t{2});
    EXPECT_EQ(v[1], 2);
}

// ---- at() 的边界检查 ----
TEST(C1_at, bounds_check) {
    Vector<int> v;
    v.push_back(7);

    EXPECT_THROW(v.at(5), std::out_of_range);   // 越界必抛（at 已给出）
    if (v.size() == 1) {                         // 实现 push_back 后才会进入
        EXPECT_NO_THROW(v.at(0));
        EXPECT_EQ(v.at(0), 7);
    }
}

// ---- 拷贝构造是深拷贝：改副本不影响原始，且内存独立 ----
TEST(C1_copy, deep_copy) {
    Vector<int> a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    Vector<int> b = a;                       // 拷贝构造
    ASSERT_EQ(b.size(), std::size_t{3});
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[2], 3);

    b[0] = 99;                               // 改副本
    EXPECT_EQ(a[0], 1);                       // 原始不变 → 深拷贝
    EXPECT_NE(a.data(), b.data());           // 两块独立内存
}

// ---- 移动构造：O(1) 接管内存，源被掏空 ----
TEST(C1_move, move_ctor_steals) {
    Vector<int> a;
    for (int i = 0; i < 4; ++i) a.push_back(i);

    ASSERT_EQ(a.size(), std::size_t{4});
    int* a_data = a.data();

    Vector<int> b = std::move(a);
    EXPECT_EQ(b.size(), std::size_t{4});
    EXPECT_EQ(b.data(), a_data);             // 直接接管同一块内存（没搬运）
    EXPECT_EQ(b[3], 3);
    EXPECT_EQ(a.size(), std::size_t{0});     // 源被掏空
    EXPECT_EQ(a.data(), static_cast<int*>(nullptr));
}

// ---- 拷贝赋值 + 移动赋值 ----
TEST(C1_assign, copy_and_move_assign) {
    Vector<int> a;
    a.push_back(1);
    a.push_back(2);
    Vector<int> b;
    b.push_back(9);

    b = a;                                   // 拷贝赋值（应先释放 b 原有的 9）
    ASSERT_EQ(b.size(), std::size_t{2});
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 2);
    EXPECT_NE(a.data(), b.data());

    Vector<int> c;
    c = std::move(b);                        // 移动赋值
    ASSERT_EQ(c.size(), std::size_t{2});
    EXPECT_EQ(c[0], 1);
    EXPECT_EQ(b.size(), std::size_t{0});     // 源被掏空
}

// ---- 生命周期：构造/析构必须配对，无泄漏 ----
TEST(C1_lifetime, no_leak_on_destroy) {
    Counted::alive = 0;
    {
        Vector<Counted> v;
        v.push_back(Counted(1));
        v.push_back(Counted(2));
        v.push_back(Counted(3));

        ASSERT_EQ(v.size(), std::size_t{3});
        EXPECT_EQ(Counted::alive, 3);        // 3 个对象活在 vector 内
    }                                        // 离开作用域 → 析构 vector
    EXPECT_EQ(Counted::alive, 0);            // 全部被析构，无泄漏
}

// ---- clear 析构所有元素但保留容量 ----
TEST(C1_lifetime, clear_destroys_keeps_capacity) {
    Counted::alive = 0;
    Vector<Counted> v;
    v.push_back(Counted(1));
    v.push_back(Counted(2));

    ASSERT_EQ(v.size(), std::size_t{2});
    EXPECT_EQ(Counted::alive, 2);

    v.clear();
    EXPECT_EQ(v.size(), std::size_t{0});
    EXPECT_EQ(Counted::alive, 0);            // 元素被析构
    EXPECT_GE(v.capacity(), std::size_t{2}); // 容量保留（内存没还）
}
