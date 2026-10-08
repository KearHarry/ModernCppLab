// =============================================================================
//  C7 测试：有序映射语义 + 每一步红黑树不变量。
//  所有读取前都先检查 size/find，骨架空树只会红测，不会解引用空指针。
// =============================================================================
#include "test_framework.hpp"
#include "red_black_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace cppbc;

using Tree = RedBlackTree<int, std::string>;

struct C7ThrowingCompare {
    static bool throw_now;

    bool operator()(int lhs, int rhs) const {
        if (throw_now) throw std::runtime_error("comparison failed");
        return lhs < rhs;
    }
};
bool C7ThrowingCompare::throw_now = false;

using ThrowingTree = RedBlackTree<int, int, C7ThrowingCompare>;

// Compare 是允许抛异常的用户扩展点，查询 API 不能错误地承诺 noexcept。
static_assert(!noexcept(std::declval<ThrowingTree&>().find(0)));
static_assert(!noexcept(std::declval<const ThrowingTree&>().find(0)));
static_assert(!noexcept(std::declval<const ThrowingTree&>().contains(0)));
static_assert(!noexcept(std::declval<const ThrowingTree&>().lower_bound_key(0)));

static std::vector<int> keys_of(const Tree& tree) {
    std::vector<int> out;
    tree.for_each([&](int key, const std::string&) { out.push_back(key); });
    return out;
}

TEST(C7_empty, empty_tree_is_valid) {
    Tree tree;
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.size(), std::size_t{0});
    EXPECT_TRUE(tree.validate().ok);
    EXPECT_TRUE(tree.find(1) == nullptr);
    EXPECT_TRUE(tree.lower_bound_key(1) == nullptr);
    EXPECT_FALSE(tree.erase(1));
}

TEST(C7_insert, inserts_and_updates_map_values) {
    Tree tree;
    EXPECT_TRUE(tree.insert_or_assign(2, "two")); // 骨架 false → 红。
    EXPECT_TRUE(tree.insert_or_assign(1, "one"));
    EXPECT_TRUE(tree.insert_or_assign(3, "three"));
    EXPECT_FALSE(tree.insert_or_assign(2, "TWO"));
    ASSERT_EQ(tree.size(), std::size_t{3});

    const std::string* value = tree.find(2);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, std::string("TWO"));
    EXPECT_TRUE(tree.validate().ok);
}

TEST(C7_order, in_order_traversal_is_sorted) {
    Tree tree;
    int input[] = {8, 3, 10, 1, 6, 14, 4, 7, 13, 2, 9, 5, 12, 11};
    for (int key : input) tree.insert_or_assign(key, std::to_string(key));
    ASSERT_EQ(tree.size(), std::size_t{14});

    std::vector<int> got = keys_of(tree);
    EXPECT_TRUE(std::is_sorted(got.begin(), got.end()));
    for (std::size_t i = 0; i < got.size(); ++i)
        EXPECT_EQ(got[i], static_cast<int>(i + 1));
}

TEST(C7_lower_bound, finds_first_not_less_than_key) {
    Tree tree;
    for (int key : {10, 20, 30, 40}) tree.insert_or_assign(key, "v");
    ASSERT_EQ(tree.size(), std::size_t{4});

    const int* a = tree.lower_bound_key(5);
    const int* b = tree.lower_bound_key(20);
    const int* c = tree.lower_bound_key(21);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(*a, 10);
    EXPECT_EQ(*b, 20);
    EXPECT_EQ(*c, 30);
    EXPECT_TRUE(tree.lower_bound_key(41) == nullptr);
}

TEST(C7_invariants, ascending_insertions_remain_balanced) {
    Tree tree;
    for (int key = 1; key <= 200; ++key) {
        ASSERT_TRUE(tree.insert_or_assign(key, std::to_string(key)));
        auto check = tree.validate();
        EXPECT_TRUE(check.ok);
        EXPECT_EQ(check.node_count, tree.size());
    }
    EXPECT_EQ(tree.size(), std::size_t{200}); // 骨架确定性红。
}

TEST(C7_erase, removes_leaf_one_child_two_children_and_root) {
    Tree tree;
    for (int key : {20, 10, 30, 5, 15, 25, 40, 1, 6, 14, 16, 35, 50})
        tree.insert_or_assign(key, std::to_string(key));
    ASSERT_EQ(tree.size(), std::size_t{13});

    for (int key : {1, 5, 30, 20}) {
        EXPECT_TRUE(tree.erase(key));
        EXPECT_FALSE(tree.contains(key));
        EXPECT_TRUE(tree.validate().ok);
    }
    EXPECT_FALSE(tree.erase(999));
    EXPECT_EQ(tree.size(), std::size_t{9});
}

TEST(C7_stress, mixed_insert_erase_preserves_all_invariants) {
    Tree tree;
    std::vector<int> keys;
    for (int i = 0; i < 127; ++i) keys.push_back(i);

    unsigned state = 0xC7C7u;
    for (int i = static_cast<int>(keys.size()) - 1; i > 0; --i) {
        state = state * 1664525u + 1013904223u;
        int j = static_cast<int>(state % static_cast<unsigned>(i + 1));
        std::swap(keys[static_cast<std::size_t>(i)], keys[static_cast<std::size_t>(j)]);
    }
    for (int key : keys) tree.insert_or_assign(key, std::to_string(key));
    ASSERT_EQ(tree.size(), std::size_t{127});
    EXPECT_TRUE(tree.validate().ok);

    for (int key = 0; key < 127; key += 2) {
        EXPECT_TRUE(tree.erase(key));
        EXPECT_TRUE(tree.validate().ok);
    }
    EXPECT_EQ(tree.size(), std::size_t{63});
    std::vector<int> got = keys_of(tree);
    ASSERT_EQ(got.size(), std::size_t{63});
    for (int key : got) EXPECT_TRUE(key % 2 == 1);
}

TEST(C7_comparator, supports_custom_order) {
    RedBlackTree<int, int, std::greater<int>> tree;
    for (int key : {1, 4, 2, 3}) tree.insert_or_assign(key, key);
    ASSERT_EQ(tree.size(), std::size_t{4});
    std::vector<int> got;
    tree.for_each([&](int key, int) { got.push_back(key); });
    ASSERT_EQ(got.size(), std::size_t{4});
    EXPECT_EQ(got[0], 4);
    EXPECT_EQ(got[3], 1);
    EXPECT_TRUE(tree.validate().ok);
}

TEST(C7_comparator, lookup_propagates_comparison_exceptions) {
    ThrowingTree tree;
    ASSERT_TRUE(tree.insert_or_assign(2, 20));
    ASSERT_TRUE(tree.insert_or_assign(1, 10));

    C7ThrowingCompare::throw_now = true;
    EXPECT_THROW(tree.find(1), std::runtime_error);
    EXPECT_THROW(static_cast<const ThrowingTree&>(tree).find(1), std::runtime_error);
    EXPECT_THROW(tree.contains(1), std::runtime_error);
    EXPECT_THROW(tree.lower_bound_key(1), std::runtime_error);
    C7ThrowingCompare::throw_now = false;
}
