#include "test_framework.hpp"
#include "iterators_ranges.hpp"

#include <algorithm>
#include <concepts>
#include <ranges>
#include <stdexcept>
#include <vector>

using namespace cppbc;

static_assert(std::random_access_iterator<StridedIterator<int>>);
static_assert(std::random_access_iterator<StridedIterator<const int>>);

TEST(C10_iterator, walks_every_second_element) {
    int values[] = {0, 10, 1, 11, 2, 12, 3, 13};
    StridedView<int> evens{values, 4, 2};
    std::vector<int> seen;
    for (int v : evens) seen.push_back(v);
    ASSERT_EQ(seen.size(), std::size_t{4});
    EXPECT_EQ(seen[0], 0);
    EXPECT_EQ(seen[1], 1);
    EXPECT_EQ(seen[2], 2);
    EXPECT_EQ(seen[3], 3);
}

TEST(C10_iterator, supports_random_access_operations) {
    int values[] = {5, 99, 6, 99, 7, 99, 8, 99, 9};
    StridedView<int> view{values, 5, 2};
    auto first = view.begin();
    auto last = view.end();
    EXPECT_EQ(last - first, std::ptrdiff_t{5});
    EXPECT_EQ(first[3], 8);
    EXPECT_EQ(*(first + 4), 9);
    EXPECT_EQ(*(--last), 9);
}

TEST(C10_iterator, works_with_standard_algorithms) {
    int values[] = {4, 0, 1, 0, 3, 0, 2, 0};
    StridedView<int> view{values, 4, 2};
    std::ranges::sort(view.begin(), view.end());
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[2], 2);
    EXPECT_EQ(values[4], 3);
    EXPECT_EQ(values[6], 4);
    EXPECT_EQ(values[1], 0); // 非视图元素不应被碰到
}

TEST(C10_const, const_iterator_reads_without_writing) {
    const int values[] = {2, -1, 4, -1, 6};
    StridedView<const int> view{values, 3, 2};
    EXPECT_EQ(std::ranges::distance(view.begin(), view.end()), std::ptrdiff_t{3});
    EXPECT_EQ(*view.begin(), 2);
    EXPECT_EQ(*(view.begin() + 2), 6);
}

TEST(C10_view, rejects_non_positive_stride) {
    int values[] = {1, 2, 3};
    EXPECT_THROW((void)StridedView<int>(values, 3, 0), std::invalid_argument);
    EXPECT_THROW((void)StridedView<int>(values, 3, -1), std::invalid_argument);
    EXPECT_THROW((void)StridedIterator<int>(values, 0), std::invalid_argument);
}

struct Record { int id; const char* name; };

TEST(C10_algorithm, lower_bound_uses_projection) {
    std::vector<Record> rows{{1,"a"}, {3,"b"}, {3,"c"}, {7,"d"}, {9,"e"}};
    auto it = projected_lower_bound(rows.begin(), rows.end(), 3,
                                    [](const Record& r) { return r.id; });
    ASSERT_TRUE(it != rows.end());
    EXPECT_EQ(it->id, 3);
    EXPECT_EQ(std::distance(rows.begin(), it), std::ptrdiff_t{1});

    auto missing = projected_lower_bound(rows.begin(), rows.end(), 8,
                                         [](const Record& r) { return r.id; });
    ASSERT_TRUE(missing != rows.end());
    EXPECT_EQ(missing->id, 9);

    auto member_projection = projected_lower_bound(rows.begin(), rows.end(), 7,
                                                    &Record::id);
    ASSERT_TRUE(member_projection != rows.end());
    EXPECT_EQ(member_projection->id, 7);
}

TEST(C10_algorithm, lower_bound_handles_boundaries) {
    std::vector<int> values{2, 4, 6};
    auto identity = [](int v) { return v; };
    EXPECT_TRUE(projected_lower_bound(values.begin(), values.end(), 1, identity)
                == values.begin());
    EXPECT_TRUE(projected_lower_bound(values.begin(), values.end(), 7, identity)
                == values.end());
}
