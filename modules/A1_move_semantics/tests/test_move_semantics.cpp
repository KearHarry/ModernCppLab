// =============================================================================
//  A1 测试文件 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  它不实现作业逻辑，只负责"检查你的 move_semantics.hpp 写对了没有"。
//
//  【整个框架怎么跑起来的？（从写到测）】
//
//    move_semantics.hpp  ← 你在这里写 Probe、relay、Buffer
//            ↑
//            │  #include
//            │
//    test_move_semantics.cpp  ← 本文件：写 TEST(...) 测试用例
//            ↑
//            │  #include
//            │
//    common/test_framework.hpp  ← 提供 TEST、EXPECT_EQ、main() 等
//
//  编译时把这三个拼成一个 .exe，运行后自动执行所有 TEST，打印通过/失败。
//
//  【TEST(套件名, 用例名) { ... }】
//  定义一个测试。例如 TEST(A1_forward, lvalue_is_copied) 表示
//  "A1_forward 套件里的 lvalue_is_copied 用例"。
//
//  【EXPECT_xxx / ASSERT_xxx】
//  - EXPECT_EQ(a, b)  ：期望 a == b，失败继续跑后面的检查
//  - ASSERT_EQ(a, b)  ：期望 a == b，失败立刻结束本用例（防止后面越界崩溃）
//  - EXPECT_TRUE(x)   ：期望 x 为真
//  - EXPECT_NE(a, b)  ：期望 a != b
//
//  【static_assert】
//  编译期检查：如果条件不成立，代码都编译不过（比运行测试更早发现问题）。
//
// =============================================================================

#include "test_framework.hpp"   // 测试框架（在仓库 common/ 目录）
#include "move_semantics.hpp"     // 你的作业代码

#include <type_traits>  // static_assert 用的编译期类型检查
#include <utility>      // std::move

using namespace cppbc;  // 之后可以直接写 Probe、Buffer，不用写 cppbc::Probe

// =============================================================================
//  测试组 1：完美转发 relay()
// =============================================================================

// 传【左值】时，应该走拷贝构造
TEST(A1_forward, lvalue_is_copied) {
    Probe lv;                    // lv 是有名字的变量 → 左值
    Probe r = relay(lv);         // relay 应该把 lv 当左值转发 → Probe 拷贝构造
    EXPECT_TRUE(r.origin == Probe::Origin::Copy);  // 期望 origin 被标成 Copy
}

// 传【右值】时，应该走移动构造
TEST(A1_forward, rvalue_is_moved) {
    // Probe{} 是临时对象 → 右值；传进 relay 后应走移动构造
    Probe r = relay(Probe{});
    EXPECT_TRUE(r.origin == Probe::Origin::Move);  // 期望 origin 被标成 Move
}

// =============================================================================
//  测试组 2：Buffer 的 Rule of Five
// =============================================================================

// 拷贝构造必须是【深拷贝】：两份独立内存，互不影响
TEST(A1_buffer, copy_ctor_is_deep) {
    Buffer a{1, 2, 3};           // a 里有 [1, 2, 3]
    Buffer b = a;                // 拷贝构造：b 是 a 的副本

    ASSERT_EQ(b.size(), std::size_t{3});  // b 长度应为 3（先 ASSERT，错了就不继续）
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[2], 3);

    b[0] = 99;                   // 只改 b
    EXPECT_EQ(a[0], 1);          // a 仍是 1 → 说明不是同一块内存
    EXPECT_NE(a.data(), b.data()); // 两个指针地址不同 → 深拷贝成功
}

// 移动构造：b 偷走 a 的指针，a 变空
TEST(A1_buffer, move_ctor_steals) {
    Buffer a{1, 2, 3};
    int* original = a.data();      // 记下 a 原来那块内存的地址
    Buffer b = std::move(a);       // std::move 告诉编译器：可以把 a 当右值"搬走"

    EXPECT_EQ(b.size(), std::size_t{3});
    EXPECT_EQ(b.data(), original); // b 用的就是 a 原来的那块内存
    EXPECT_TRUE(a.empty());        // a 被搬空了
    EXPECT_EQ(a.data(), static_cast<int*>(nullptr)); // a 的指针应为空
}

// 拷贝赋值：b = a 也要深拷贝，且 a = a 不能崩
TEST(A1_buffer, copy_assign_deep_and_self_safe) {
    Buffer a{1, 2, 3};
    Buffer b;                      // 空 Buffer
    b = a;                         // 拷贝赋值（不是构造，是已存在的 b 被覆盖）

    ASSERT_EQ(b.size(), std::size_t{3});
    EXPECT_EQ(b[1], 2);
    b[1] = 50;
    EXPECT_EQ(a[1], 2);            // 仍独立

    a = a;                         // 自赋值：必须安全，数据不能丢
    ASSERT_EQ(a.size(), std::size_t{3});
    EXPECT_EQ(a[1], 2);
}

// 移动赋值：b 扔掉自己的旧内存，偷 a 的
TEST(A1_buffer, move_assign_steals) {
    Buffer a{1, 2, 3};
    int* original = a.data();
    Buffer b{9, 9};                // b 里原来有 [9, 9]
    b = std::move(a);              // b 应先释放自己的内存，再接管 a 的

    EXPECT_EQ(b.size(), std::size_t{3});
    EXPECT_EQ(b.data(), original);
    EXPECT_TRUE(a.empty());
}

// =============================================================================
//  编译期检查：移动操作必须标 noexcept
// =============================================================================
//
//  标准库的 vector 扩容时：如果元素的移动是 noexcept，就用移动（快）；
//  否则会退回拷贝（更安全）。所以大厂面试常问为什么要写 noexcept。
//
static_assert(std::is_nothrow_move_constructible_v<Buffer>,
              "Buffer 的移动构造应当是 noexcept");
static_assert(std::is_nothrow_move_assignable_v<Buffer>,
              "Buffer 的移动赋值应当是 noexcept");
