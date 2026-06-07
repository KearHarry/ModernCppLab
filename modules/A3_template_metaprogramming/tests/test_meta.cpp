// =============================================================================
//  A3 测试文件 —— 检查你的 meta.hpp 写对了没有
// -----------------------------------------------------------------------------
//  注意：trait 的结果是"编译期常量"，但我们用运行期 EXPECT_* 来检查它，
//  这样骨架阶段也能编译通过、看到红色，再逐个写绿。
//
//  小坑：宏里出现 <A, B> 的逗号会被预处理器误当成"两个宏参数"，
//  所以遇到带逗号的类型表达式要多包一层括号： EXPECT_TRUE(( ... ))。
// =============================================================================
#include "test_framework.hpp"
#include "meta.hpp"

#include <string>
#include <type_traits>

using namespace cppbc;

// 一个"不能相加"的类型，用来检验 Addable 概念会正确判 false。
struct NotAddable {};

// ---- RemoveReference：剥引用 ----
TEST(A3_traits, remove_reference) {
    EXPECT_TRUE((std::is_same_v<RemoveReferenceT<int>,   int>));   // 非引用：原样
    EXPECT_TRUE((std::is_same_v<RemoveReferenceT<int&>,  int>));   // 左值引用 → int
    EXPECT_TRUE((std::is_same_v<RemoveReferenceT<int&&>, int>));   // 右值引用 → int
}

// ---- IsSame：类型相等 ----
TEST(A3_traits, is_same) {
    EXPECT_TRUE((IsSameV<int, int>));
    EXPECT_TRUE((IsSameV<double, double>));
    EXPECT_FALSE((IsSameV<int, double>));
    EXPECT_FALSE((IsSameV<int, int&>));     // int 和 int& 不是同一类型
}

// ---- Conditional：编译期三元 ----
TEST(A3_traits, conditional) {
    EXPECT_TRUE((std::is_same_v<ConditionalT<true,  int, double>, int>));
    EXPECT_TRUE((std::is_same_v<ConditionalT<false, int, double>, double>));
}

// ---- HasSize：SFINAE 成员检测 ----
TEST(A3_traits, has_size_detector) {
    EXPECT_TRUE(HasSizeV<std::string>);     // string 有 .size()
    EXPECT_TRUE((HasSizeV<std::string>));
    EXPECT_FALSE(HasSizeV<int>);            // int 没有 .size()
    EXPECT_FALSE(HasSizeV<double>);
}

// ---- 折叠表达式：求和 ----
TEST(A3_variadic, sum_fold) {
    EXPECT_EQ(sum(1, 2, 3, 4), 10);
    EXPECT_EQ(sum(42), 42);
    EXPECT_EQ(sum(), 0);                    // 空包应为 0（提示你用二元折叠）
}

// ---- 折叠表达式：逻辑与 ----
TEST(A3_variadic, all_of_fold) {
    EXPECT_TRUE(all_of(true, true, true));
    EXPECT_FALSE(all_of(true, false, true));
    EXPECT_TRUE(all_of());                  // 空包：&& 的单位元是 true
}

// ---- sizeof... 参数计数 ----
TEST(A3_variadic, count_args) {
    EXPECT_EQ(count(), std::size_t{0});
    EXPECT_EQ(count(1, 'a', 3.0), std::size_t{3});
    EXPECT_EQ(count(1, 2, 3, 4, 5), std::size_t{5});
}

// ---- C++20 concept ----
TEST(A3_concept, addable) {
    EXPECT_TRUE(Addable<int>);
    EXPECT_TRUE(Addable<double>);
    EXPECT_FALSE(Addable<NotAddable>);      // 没有 operator+ → 不满足
    EXPECT_EQ(twice(21), 42);               // 受 Addable 约束的函数正常工作
}
