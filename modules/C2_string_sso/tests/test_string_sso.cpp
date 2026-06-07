// =============================================================================
//  C2 测试文件 —— 检查你的 string_sso.hpp 写对了没有
// -----------------------------------------------------------------------------
//  设计说明：凡是要按下标读取字符内容的用例，先用 ASSERT_EQ 校验 size()。
//  这样骨架阶段（构造/追加都还是空操作、size 为 0）会先中止，绝不越界读内联缓冲，
//  所以一开始只会变红、不会崩溃。is_small() 与 strcmp 是只读的，可直接断言。
// =============================================================================
#include "test_framework.hpp"
#include "string_sso.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

using namespace cppbc;

// ---- 短字符串走内联（SSO 生效，无堆分配） ----
TEST(C2_small, short_is_inline) {
    String s("hello");
    EXPECT_EQ(s.size(), std::size_t{5});
    EXPECT_TRUE(s.is_small());                       // 短串：内联存储
    EXPECT_EQ(std::strcmp(s.c_str(), "hello"), 0);
}

// ---- 长字符串走堆 ----
TEST(C2_large, long_is_heap) {
    const char* lit = "this string is definitely longer than fifteen chars";
    String s(lit);
    EXPECT_EQ(s.size(), std::strlen(lit));
    EXPECT_FALSE(s.is_small());                      // 长串：堆存储
    EXPECT_EQ(std::strcmp(s.c_str(), lit), 0);
}

// ---- SSO 边界：恰好 15 字符内联，16 字符转堆 ----
TEST(C2_boundary, fifteen_inline_sixteen_heap) {
    String s15("012345678901234");                   // 15 字符
    EXPECT_EQ(s15.size(), std::size_t{15});
    EXPECT_TRUE(s15.is_small());

    String s16("0123456789012345");                  // 16 字符
    EXPECT_EQ(s16.size(), std::size_t{16});
    EXPECT_FALSE(s16.is_small());
}

// ---- push_back 超过内联容量后自动转堆，内容与 '\0' 收尾都对 ----
TEST(C2_push, push_back_grows_to_heap) {
    String s;
    EXPECT_TRUE(s.is_small());
    for (int i = 0; i < 20; ++i) s.push_back('a');

    ASSERT_EQ(s.size(), std::size_t{20});            // 骨架阶段在此中止，避免越界读
    EXPECT_FALSE(s.is_small());                      // 20 > 15 → 已转堆
    bool all_a = true;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (s.c_str()[i] != 'a') all_a = false;
    EXPECT_TRUE(all_a);
    EXPECT_EQ(s.c_str()[20], '\0');                  // C 串以 '\0' 收尾
}

// ---- append 跨越内联/堆边界拼接，内容正确 ----
TEST(C2_append, append_across_boundary) {
    String s("short");
    s.append("___");
    s.append(" plus a long tail to exceed the inline buffer capacity");

    const char* expected =
        "short___ plus a long tail to exceed the inline buffer capacity";
    ASSERT_EQ(s.size(), std::strlen(expected));
    EXPECT_EQ(std::strcmp(s.c_str(), expected), 0);
    EXPECT_FALSE(s.is_small());
}

// ---- 拷贝短串：深拷贝、各用各的内联缓冲、互不影响 ----
TEST(C2_copy, deep_copy_small) {
    String a("hello");
    String b = a;

    ASSERT_EQ(b.size(), std::size_t{5});
    EXPECT_EQ(std::strcmp(b.c_str(), "hello"), 0);
    EXPECT_TRUE(b.is_small());
    EXPECT_NE(a.c_str(), b.c_str());                 // 指向各自的 buf_，地址不同

    b[0] = 'J';                                       // 改 b
    EXPECT_EQ(a.c_str()[0], 'h');                     // a 不受影响 → 深拷贝
}

// ---- 拷贝长串：深拷贝、各自独立堆 ----
TEST(C2_copy, deep_copy_large) {
    const char* lit = "a sufficiently long string for heap allocation test!!";
    String a(lit);
    String b = a;

    ASSERT_EQ(b.size(), std::strlen(lit));
    EXPECT_EQ(std::strcmp(b.c_str(), lit), 0);
    EXPECT_FALSE(b.is_small());
    EXPECT_NE(a.c_str(), b.c_str());                 // 两块独立堆内存
}

// ---- 移动短串：内容被拷进目标自己的 buf_（短串无法偷指针） ----
TEST(C2_move, move_small_copies_inline) {
    String a("hello");
    String b = std::move(a);

    ASSERT_EQ(b.size(), std::size_t{5});
    EXPECT_EQ(std::strcmp(b.c_str(), "hello"), 0);
    EXPECT_TRUE(b.is_small());                       // 仍是短串，data_ 指向 b 自己的 buf_
}

// ---- 移动长串：直接接管堆指针（O(1)），源被复位为空短串 ----
TEST(C2_move, move_large_steals_heap) {
    const char* lit = "a sufficiently long string for heap allocation test!!";
    String a(lit);
    const char* a_heap = a.c_str();                  // 记下 a 的堆地址

    String b = std::move(a);
    ASSERT_EQ(b.size(), std::strlen(lit));
    EXPECT_FALSE(b.is_small());
    EXPECT_EQ(b.c_str(), a_heap);                    // b 接管了同一块堆（没拷贝）
    EXPECT_TRUE(a.is_small());                       // a 被复位成空短串
    EXPECT_EQ(a.size(), std::size_t{0});
}

// ---- 拷贝赋值 + 移动赋值（含长串，覆盖释放旧堆的路径） ----
TEST(C2_assign, copy_and_move_assign) {
    const char* lit = "hello world, long enough to live on the heap for sure";
    String a(lit);
    String b("x");
    b = a;                                            // 拷贝赋值（b 原是短串）
    ASSERT_EQ(b.size(), std::strlen(lit));
    EXPECT_EQ(std::strcmp(b.c_str(), lit), 0);
    EXPECT_NE(a.c_str(), b.c_str());

    String c("y");
    c = std::move(b);                                 // 移动赋值
    ASSERT_EQ(c.size(), std::strlen(lit));
    EXPECT_EQ(std::strcmp(c.c_str(), lit), 0);
}
