// =============================================================================
//  A8 测试：未完成的值构造、移动赋值与 visit 使用安全占位；危险访问前均有保护。
// =============================================================================
#include "test_framework.hpp"
#include "variant.hpp"

#include <string>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace cppbc;

struct ToText {
    std::string operator()(int value) const { return std::to_string(value); }
    std::string operator()(const std::string& value) const { return value; }
    std::string operator()(std::string& value) const { return value; }
};

TEST(A8_default, starts_empty_and_checked_access_throws) {
    IntStringVariant v;
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(v.kind() == IntStringVariant::Kind::Empty);
    EXPECT_THROW(v.get_int(), BadVariantAccess);
    EXPECT_THROW(v.get_string(), BadVariantAccess);
    EXPECT_THROW(v.visit(ToText{}), BadVariantAccess);
}

TEST(A8_construct, int_and_string_activate_correct_member) {
    IntStringVariant a(42);
    EXPECT_TRUE(a.holds_int());
    ASSERT_TRUE(a.get_if_int() != nullptr);
    EXPECT_EQ(*a.get_if_int(), 42);
    EXPECT_TRUE(a.get_if_string() == nullptr);

    IntStringVariant b(std::string("hello"));
    EXPECT_TRUE(b.holds_string());
    ASSERT_TRUE(b.get_if_string() != nullptr);
    EXPECT_EQ(*b.get_if_string(), std::string("hello"));
    EXPECT_TRUE(b.get_if_int() == nullptr);
}

TEST(A8_construct, null_c_string_is_rejected_without_touching_union_storage) {
    EXPECT_THROW(IntStringVariant(nullptr), std::invalid_argument);

    IntStringVariant v;
    v.emplace_int(7);
    EXPECT_THROW(v.emplace_string(nullptr), std::invalid_argument);
    EXPECT_TRUE(v.holds_int());
    EXPECT_EQ(v.get_int(), 7);
}

TEST(A8_emplace, switches_active_type_and_reset_is_idempotent) {
    IntStringVariant v;
    v.emplace_string("abc");
    ASSERT_TRUE(v.get_if_string() != nullptr);
    EXPECT_EQ(v.get_string(), std::string("abc"));

    v.emplace_int(7); // 必须先销毁 string，再激活 int
    ASSERT_TRUE(v.get_if_int() != nullptr);
    EXPECT_EQ(v.get_int(), 7);
    EXPECT_TRUE(v.get_if_string() == nullptr);

    v.reset();
    EXPECT_TRUE(v.empty());
    v.reset();
    EXPECT_TRUE(v.empty());
}

TEST(A8_copy, copies_value_without_aliasing) {
    IntStringVariant original;
    original.emplace_string("source");
    IntStringVariant copy(original);
    ASSERT_TRUE(copy.get_if_string() != nullptr);
    EXPECT_EQ(copy.get_string(), std::string("source"));

    copy.get_string()[0] = 'S';
    EXPECT_EQ(original.get_string(), std::string("source"));
    EXPECT_EQ(copy.get_string(), std::string("Source"));
}

TEST(A8_move, transfers_value_and_empties_source) {
    IntStringVariant source;
    source.emplace_string("payload");
    IntStringVariant target(std::move(source));
    EXPECT_TRUE(source.empty());
    ASSERT_TRUE(target.get_if_string() != nullptr);
    EXPECT_EQ(target.get_string(), std::string("payload"));
}

TEST(A8_assign, supports_same_type_switching_and_self_assignment) {
    IntStringVariant a;
    a.emplace_int(1);
    IntStringVariant b;
    b.emplace_string("two");
    a = b;
    ASSERT_TRUE(a.get_if_string() != nullptr);
    EXPECT_EQ(a.get_string(), std::string("two"));

    a = a;
    EXPECT_EQ(a.get_string(), std::string("two"));

    IntStringVariant number;
    number.emplace_int(9);
    b = number;
    ASSERT_TRUE(b.get_if_int() != nullptr);
    EXPECT_EQ(b.get_int(), 9);
}

TEST(A8_assign, move_assignment_transfers_value_and_empties_named_source) {
    IntStringVariant source;
    source.emplace_string("move-source");
    IntStringVariant target;
    target.emplace_int(7);

    target = std::move(source);

    EXPECT_TRUE(source.empty());
    ASSERT_TRUE(target.get_if_string() != nullptr);
    EXPECT_EQ(target.get_string(), std::string("move-source"));
}

TEST(A8_visit, dispatches_by_active_type_and_returns_common_result) {
    IntStringVariant number;
    number.emplace_int(123);
    IntStringVariant text;
    text.emplace_string("xyz");
    EXPECT_EQ(number.visit(ToText{}), std::string("123"));
    EXPECT_EQ(text.visit(ToText{}), std::string("xyz"));

    number.visit([](auto& value) {
        using Value = std::remove_reference_t<decltype(value)>;
        if constexpr (!std::is_const_v<Value> &&
                      std::is_same_v<std::remove_cv_t<Value>, int>) {
            value = 8;
        }
    });
    EXPECT_EQ(number.get_int(), 8);
}

TEST(A8_const, const_access_and_visit_work) {
    IntStringVariant prepared;
    prepared.emplace_string("const");
    const IntStringVariant v(prepared);
    static_assert(std::is_same_v<decltype(v.get_if_string()), const std::string*>);
    ASSERT_TRUE(v.get_if_string() != nullptr);
    EXPECT_EQ(v.visit(ToText{}), std::string("const"));
}
