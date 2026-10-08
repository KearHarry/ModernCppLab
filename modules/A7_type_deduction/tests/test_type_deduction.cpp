// =============================================================================
//  A7 测试：用运行期标签验证编译期类型推导，并覆盖引用返回的真实修改效果。
// =============================================================================
#include "test_framework.hpp"
#include "type_deduction.hpp"

#include <type_traits>
#include <vector>

using namespace cppbc;

namespace {
int twice(int x) { return x * 2; }
}

TEST(A7_type_tag, distinguishes_cv_ref_pointer_array_and_function) {
    EXPECT_TRUE(type_tag<int>() == TypeTag::Int);
    EXPECT_TRUE(type_tag<const int>() == TypeTag::ConstInt);
    EXPECT_TRUE(type_tag<int&>() == TypeTag::IntLRef);
    EXPECT_TRUE(type_tag<const int&>() == TypeTag::ConstIntLRef);
    EXPECT_TRUE(type_tag<int&&>() == TypeTag::IntRRef);
    EXPECT_TRUE(type_tag<int*>() == TypeTag::IntPointer);
    EXPECT_TRUE(type_tag<const int*>() == TypeTag::ConstIntPointer);
    EXPECT_TRUE(type_tag<int[3]>() == TypeTag::IntArray3);
    EXPECT_TRUE(type_tag<int(int)>() == TypeTag::IntFunction);
    EXPECT_TRUE(type_tag<int(*)(int)>() == TypeTag::IntFunctionPointer);
}

TEST(A7_by_value, drops_top_const_and_decays) {
    int x = 1;
    const int cx = 2;
    int a[3] = {1, 2, 3};

    EXPECT_TRUE(deduce_by_value(x) == TypeTag::Int);
    EXPECT_TRUE(deduce_by_value(cx) == TypeTag::Int);       // 顶层 const 丢失
    EXPECT_TRUE(deduce_by_value(a) == TypeTag::IntPointer); // 数组退化
    EXPECT_TRUE(deduce_by_value(twice) == TypeTag::IntFunctionPointer);
}

TEST(A7_by_lref, preserves_const_array_and_function) {
    int x = 1;
    const int cx = 2;
    int a[3] = {1, 2, 3};

    EXPECT_TRUE(deduce_by_lref(x) == TypeTag::IntLRef);
    EXPECT_TRUE(deduce_by_lref(cx) == TypeTag::ConstIntLRef);
    // T 推成 int[3]，参数类型是 int (&)[3]；用 remove_reference 可看见数组本体。
    EXPECT_TRUE(deduce_by_lref(a) == TypeTag::IntArray3);
    EXPECT_TRUE(deduce_by_lref(twice) == TypeTag::IntFunction);
}

TEST(A7_forwarding_reference, remembers_original_value_category) {
    int x = 7;
    const int cx = 8;

    EXPECT_TRUE(deduce_by_forward(x) == TypeTag::IntLRef);
    EXPECT_TRUE(deduce_by_forward(cx) == TypeTag::ConstIntLRef);
    EXPECT_TRUE(deduce_by_forward(9) == TypeTag::IntRRef);
}

TEST(A7_decltype_auto, element_returns_real_reference) {
    std::vector<int> values{10, 20, 30};
    auto&& selected = element(values, 1); // 必须绑定到容器中的真实元素。

    EXPECT_TRUE((std::is_lvalue_reference_v<decltype(element(values, 0))>));
    selected = 99;
    EXPECT_EQ(values[1], 99);              // 引用返回应修改原容器。
}

TEST(A7_decltype, unparenthesized_name_differs_from_expression) {
    int x = 3;
    EXPECT_TRUE(decltype_name(x) == TypeTag::IntLRef);
    // 函数形参 value 是“有名字的表达式”，所以 (value) 永远是左值表达式。
    EXPECT_TRUE(decltype_expr(x) == TypeTag::IntLRef);
    EXPECT_TRUE(decltype_name(3) == TypeTag::IntRRef);
    EXPECT_TRUE(decltype_expr(3) == TypeTag::IntLRef);
}
