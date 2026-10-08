// =============================================================================
//  C8 测试：双端增长、跨块索引、map 扩张、地址稳定性与对象生命周期。
//  读取前均先 ASSERT size，骨架插入为空操作时不会解引用空块。
// =============================================================================
#include "test_framework.hpp"
#include "segmented_deque.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>

using namespace cppbc;

TEST(C8_empty, initial_state_and_bounds) {
    SegmentedDeque<int> deque;
    EXPECT_TRUE(deque.empty());
    EXPECT_EQ(deque.size(), std::size_t{0});
    EXPECT_EQ(deque.allocated_blocks(), std::size_t{0});
    EXPECT_FALSE(deque.pop_front());
    EXPECT_FALSE(deque.pop_back());
    EXPECT_THROW(deque.at(0), std::out_of_range);
}

TEST(C8_push_back, crosses_multiple_blocks) {
    SegmentedDeque<int, 4> deque;
    for (int i = 0; i < 11; ++i) deque.push_back(i * 10);

    ASSERT_EQ(deque.size(), std::size_t{11}); // 骨架 size=0，在此安全中止。
    EXPECT_EQ(deque.allocated_blocks(), std::size_t{3});
    EXPECT_EQ(deque.front(), 0);
    EXPECT_EQ(deque[3], 30);
    EXPECT_EQ(deque[4], 40);                  // 正好跨块边界。
    EXPECT_EQ(deque[10], 100);
    EXPECT_EQ(deque.back(), 100);
}

TEST(C8_push_front, crosses_multiple_blocks_and_keeps_order) {
    SegmentedDeque<int, 3> deque;
    for (int i = 1; i <= 8; ++i) deque.push_front(i);

    ASSERT_EQ(deque.size(), std::size_t{8});
    EXPECT_GE(deque.allocated_blocks(), std::size_t{3});
    for (int i = 0; i < 8; ++i) EXPECT_EQ(deque[static_cast<std::size_t>(i)], 8 - i);
    EXPECT_EQ(deque.front(), 8);
    EXPECT_EQ(deque.back(), 1);
}

TEST(C8_mixed, front_and_back_share_one_logical_sequence) {
    SegmentedDeque<int, 4> deque;
    deque.push_back(0);
    for (int i = 1; i <= 6; ++i) {
        deque.push_front(-i);
        deque.push_back(i);
    }

    ASSERT_EQ(deque.size(), std::size_t{13});
    for (int i = 0; i < 13; ++i) EXPECT_EQ(deque[static_cast<std::size_t>(i)], i - 6);
    EXPECT_EQ(deque.at(6), 0);
    EXPECT_THROW(deque.at(13), std::out_of_range);
}

TEST(C8_pop, releases_edge_blocks_and_preserves_middle) {
    SegmentedDeque<int, 4> deque;
    for (int i = 0; i < 12; ++i) deque.push_back(i);
    ASSERT_EQ(deque.size(), std::size_t{12});
    EXPECT_EQ(deque.allocated_blocks(), std::size_t{3});

    for (int i = 0; i < 5; ++i) EXPECT_TRUE(deque.pop_front());
    for (int i = 0; i < 3; ++i) EXPECT_TRUE(deque.pop_back());
    ASSERT_EQ(deque.size(), std::size_t{4});
    EXPECT_EQ(deque.front(), 5);
    EXPECT_EQ(deque.back(), 8);
    EXPECT_EQ(deque.allocated_blocks(), std::size_t{1});
}

TEST(C8_map_growth, grows_pointer_map_without_moving_elements) {
    SegmentedDeque<int, 2> deque;
    for (int i = 0; i < 4; ++i) deque.push_back(i);
    ASSERT_EQ(deque.size(), std::size_t{4});

    int* stable = &deque[2];
    std::size_t old_map_capacity = deque.map_capacity();
    for (int i = 4; i < 40; ++i) deque.push_back(i);
    for (int i = 1; i <= 20; ++i) deque.push_front(-i);

    EXPECT_GT(deque.map_capacity(), old_map_capacity);
    EXPECT_EQ(&deque[22], stable); // 前面新增 20 个，原下标 2 变为 22；地址不变。
    EXPECT_EQ(*stable, 2);
}

struct C8Counted {
    static int alive;
    int value = 0;

    C8Counted() { ++alive; }
    explicit C8Counted(int v) : value(v) { ++alive; }
    C8Counted(const C8Counted& other) : value(other.value) { ++alive; }
    C8Counted(C8Counted&& other) noexcept : value(other.value) {
        ++alive;
        other.value = -1;
    }
    ~C8Counted() { --alive; }
};
int C8Counted::alive = 0;

TEST(C8_lifetime, constructs_and_destroys_exactly_live_elements) {
    C8Counted::alive = 0;
    {
        SegmentedDeque<C8Counted, 2> deque;
        for (int i = 0; i < 7; ++i) deque.emplace_back(i);
        EXPECT_EQ(deque.size(), std::size_t{7});
        EXPECT_EQ(C8Counted::alive, 7);
        EXPECT_TRUE(deque.pop_front());
        EXPECT_TRUE(deque.pop_back());
        EXPECT_EQ(C8Counted::alive, 5);
        deque.clear();
        EXPECT_EQ(C8Counted::alive, 0);
        EXPECT_TRUE(deque.empty());
    }
    EXPECT_EQ(C8Counted::alive, 0);
}

TEST(C8_move_only, emplace_supports_noncopyable_values) {
    SegmentedDeque<std::unique_ptr<int>, 2> deque;
    deque.push_back(std::make_unique<int>(7));
    deque.emplace_front(new int(3));
    ASSERT_EQ(deque.size(), std::size_t{2});
    ASSERT_TRUE(deque.front() != nullptr);
    ASSERT_TRUE(deque.back() != nullptr);
    EXPECT_EQ(*deque.front(), 3);
    EXPECT_EQ(*deque.back(), 7);
}

struct C8NoDefault {
    explicit C8NoDefault(int v) : value(v) {}
    int value;
};

TEST(C8_emplace, supports_non_default_constructible_values) {
    SegmentedDeque<C8NoDefault, 2> deque;
    deque.emplace_back(7);
    ASSERT_EQ(deque.size(), std::size_t{1});
    EXPECT_EQ(deque.front().value, 7);
}

struct C8Throwing {
    static bool throw_now;
    static int alive;

    explicit C8Throwing(int v) : value(v) {
        if (throw_now) throw std::runtime_error("C8Throwing constructor");
        ++alive;
    }
    C8Throwing(const C8Throwing&) = delete;
    C8Throwing& operator=(const C8Throwing&) = delete;
    ~C8Throwing() { --alive; }

    int value;
};

bool C8Throwing::throw_now = false;
int C8Throwing::alive = 0;

TEST(C8_exception_safety, failed_emplace_keeps_the_logical_sequence) {
    C8Throwing::throw_now = false;
    C8Throwing::alive = 0;
    {
        // BlockSize=1，并从初始中央槽向前填满，令下一次 front 插入先扩 map。
        SegmentedDeque<C8Throwing, 1> deque;
        deque.emplace_back(0);
        for (int i = 1; i <= 4; ++i) deque.emplace_front(-i);
        ASSERT_EQ(deque.size(), std::size_t{5});
        ASSERT_EQ(C8Throwing::alive, 5);

        const std::size_t old_capacity = deque.map_capacity();
        C8Throwing::throw_now = true;
        EXPECT_THROW(deque.emplace_front(-5), std::runtime_error);
        C8Throwing::throw_now = false;

        // grow_map 已成功时容量允许变化；已存在元素、size 与生命周期不能变化。
        EXPECT_GE(deque.map_capacity(), old_capacity);
        ASSERT_EQ(deque.size(), std::size_t{5});
        EXPECT_EQ(C8Throwing::alive, 5);
        for (int i = 0; i < 5; ++i) {
            EXPECT_EQ(deque[static_cast<std::size_t>(i)].value, i - 4);
        }
    }
    EXPECT_EQ(C8Throwing::alive, 0);
}
