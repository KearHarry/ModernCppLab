// =============================================================================
//  A10 测试：简单捕获 lambda 使用安全占位值；泛型/invoke 设施未实现时抛教学异常，
//  测试框架会把它记录为失败而非崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "lambda_invoke.hpp"

#include <functional>
#include <string>
#include <type_traits>

using namespace cppbc;

namespace {
int add(int a, int b) { return a + b; }

struct Widget {
    int value = 0;
    int add_to(int delta) { value += delta; return value; }
    int read() const noexcept { return value; }
};

struct Multiplier {
    int factor;
    int operator()(int value) const { return factor * value; }
};

struct NonDefaultScaleResult {
    explicit NonDefaultScaleResult(int v) : value(v) {}
    int value;
};

struct NonDefaultScaleInput {};
NonDefaultScaleResult operator*(NonDefaultScaleInput, double) {
    return NonDefaultScaleResult{1};
}
}

TEST(A10_capture, mutable_value_capture_keeps_private_state) {
    auto counter = make_counter(10, 2);
    EXPECT_EQ(counter(), 10);
    EXPECT_EQ(counter(), 12);

    auto copy = counter;
    EXPECT_EQ(counter(), 14);
    EXPECT_EQ(copy(), 14); // 拷贝时复制捕获状态，此后各自推进
    EXPECT_EQ(copy(), 16);
    EXPECT_EQ(counter(), 16);
}

TEST(A10_capture, reference_capture_updates_original) {
    int value = 5;
    auto update = make_reference_updater(value, 3);
    EXPECT_EQ(update(), 8);
    EXPECT_EQ(value, 8);
    EXPECT_EQ(update(), 11);
    EXPECT_EQ(value, 11);
}

TEST(A10_generic_lambda, one_closure_handles_multiple_types) {
    auto scale = make_scaler(2.5);
    static_assert(std::is_same_v<decltype(scale(NonDefaultScaleInput{})),
                                 NonDefaultScaleResult>);
    EXPECT_EQ(scale(4), 10.0);
    EXPECT_EQ(scale(1.2), 3.0);
    static_assert(std::is_same_v<decltype(scale(4)), double>);
}

TEST(A10_invoke_member, calls_member_function_on_object_pointer_and_refwrap) {
    Widget w{10};
    EXPECT_EQ(invoke_member(&Widget::add_to, w, 5), 15);
    EXPECT_EQ(invoke_member(&Widget::read, &w), 15);
    EXPECT_EQ(invoke_member(&Widget::add_to, std::ref(w), 2), 17);
}

TEST(A10_invoke_member, data_member_pointer_returns_assignable_reference) {
    Widget w{7};
    int& member = invoke_member(&Widget::value, w);
    member = 99;
    EXPECT_EQ(w.value, 99);
}

TEST(A10_invoke_box, handles_free_function_lambda_and_functor) {
    auto function_box = make_invoke_box(&add);
    EXPECT_EQ(function_box(2, 3), 5);
    EXPECT_EQ(function_box.call_count(), std::size_t{1});

    int base = 10;
    auto lambda_box = make_invoke_box([base](int x) { return base + x; });
    EXPECT_EQ(lambda_box(7), 17);

    auto functor_box = make_invoke_box(Multiplier{4});
    EXPECT_EQ(functor_box(6), 24);
}

TEST(A10_invoke_box, supports_member_pointer_and_void_return) {
    Widget w{1};
    auto member_box = make_invoke_box(&Widget::add_to);
    EXPECT_EQ(member_box(w, 4), 5);
    EXPECT_EQ(member_box.call_count(), std::size_t{1});

    int observed = 0;
    auto void_box = make_invoke_box([&observed](int x) { observed = x; });
    void_box(42);
    EXPECT_EQ(observed, 42);
    EXPECT_EQ(void_box.call_count(), std::size_t{1});
}

TEST(A10_invoke_box, preserves_reference_return) {
    Widget w{3};
    auto data_box = make_invoke_box(&Widget::value);
    static_assert(std::is_same_v<decltype(data_box(w)), int&>);
    data_box(w) = 8;
    EXPECT_EQ(w.value, 8);
}
