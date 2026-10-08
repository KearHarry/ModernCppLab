#include "test_framework.hpp"
#include "sparse_set.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

using namespace cppbc;

struct Position { float x = 0; float y = 0; };

struct NonDefaultTracked {
    static int alive;
    int value;

    explicit NonDefaultTracked(int v) : value(v) { ++alive; }
    NonDefaultTracked(const NonDefaultTracked& other) : value(other.value) { ++alive; }
    NonDefaultTracked(NonDefaultTracked&& other) noexcept : value(other.value) { ++alive; }
    NonDefaultTracked& operator=(const NonDefaultTracked&) = default;
    NonDefaultTracked& operator=(NonDefaultTracked&&) noexcept = default;
    ~NonDefaultTracked() { --alive; }
};
int NonDefaultTracked::alive = 0;

TEST(E6_basic, insert_get_and_update) {
    SparseSet<Position> positions;
    positions.insert(42, Position{1, 2});
    positions.insert(7, Position{3, 4});
    ASSERT_EQ(positions.size(), std::size_t{2});
    ASSERT_TRUE(positions.contains(42));
    ASSERT_TRUE(positions.get(7) != nullptr);
    EXPECT_EQ(positions.get(42)->x, 1.0f);
    EXPECT_EQ(positions.get(7)->y, 4.0f);

    positions.insert(42, Position{9, 8});
    EXPECT_EQ(positions.size(), std::size_t{2});
    EXPECT_EQ(positions.get(42)->x, 9.0f);
}

TEST(E6_sparse, large_entity_id_does_not_create_dense_holes) {
    SparseSet<int> set;
    set.insert(1'000'000, 11);
    EXPECT_EQ(set.size(), std::size_t{1});
    EXPECT_TRUE(set.contains(1'000'000));
    EXPECT_FALSE(set.contains(999'999));
    EXPECT_EQ(set.components().size(), std::size_t{1});
}

TEST(E6_erase, swap_and_pop_repairs_moved_index) {
    SparseSet<std::string> set;
    set.insert(10, "ten");
    set.insert(20, "twenty");
    set.insert(30, "thirty");
    ASSERT_EQ(set.size(), std::size_t{3});

    EXPECT_TRUE(set.erase(20));
    EXPECT_EQ(set.size(), std::size_t{2});
    EXPECT_EQ(set.components().size(), set.size());
    EXPECT_EQ(set.entities().size(), set.size());
    EXPECT_FALSE(set.contains(20));
    ASSERT_TRUE(set.get(30) != nullptr);
    EXPECT_EQ(*set.get(30), std::string("thirty"));
    EXPECT_FALSE(set.erase(20));
}

TEST(E6_iteration, dense_iteration_visits_each_component) {
    SparseSet<int> set;
    set.insert(2, 20);
    set.insert(8, 80);
    set.insert(5, 50);
    int sum = 0;
    std::uint32_t id_sum = 0;
    set.each([&](Entity entity, int& component) {
        id_sum += entity;
        sum += component;
        component += 1;
    });
    EXPECT_EQ(id_sum, std::uint32_t{15});
    EXPECT_EQ(sum, 150);
    ASSERT_TRUE(set.get(8) != nullptr);
    EXPECT_EQ(*set.get(8), 81);
}

TEST(E6_layout, entities_and_components_are_dense_and_parallel) {
    SparseSet<Position> set;
    for (Entity e = 0; e < 32; ++e) set.insert(e * 3, Position{float(e), -float(e)});
    ASSERT_EQ(set.entities().size(), set.components().size());
    EXPECT_EQ(set.size(), std::size_t{32});
    for (std::size_t i = 0; i < set.size(); ++i) {
        const Position* p = set.get(set.entities()[i]);
        ASSERT_TRUE(p == &set.components()[i]);
    }
}

TEST(E6_lifetime, erase_destroys_removed_component) {
    NonDefaultTracked::alive = 0;
    {
        SparseSet<NonDefaultTracked> set;
        set.insert(1, NonDefaultTracked{10});
        set.insert(2, NonDefaultTracked{20});
        ASSERT_EQ(set.size(), std::size_t{2});
        EXPECT_EQ(set.components().size(), std::size_t{2});
        EXPECT_EQ(NonDefaultTracked::alive, 2);

        EXPECT_TRUE(set.erase(1));
        EXPECT_EQ(set.size(), std::size_t{1});
        EXPECT_EQ(set.components().size(), std::size_t{1});
        EXPECT_EQ(set.entities().size(), std::size_t{1});
        EXPECT_EQ(NonDefaultTracked::alive, 1);
        ASSERT_TRUE(set.get(2) != nullptr);
        EXPECT_EQ(set.get(2)->value, 20);
    }
    EXPECT_EQ(NonDefaultTracked::alive, 0);
}
