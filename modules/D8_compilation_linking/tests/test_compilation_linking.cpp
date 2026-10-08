// =============================================================================
//  D8 测试：链接成功本身就是实验的一部分；运行期再观察 linkage 身份。
// =============================================================================
#include "test_framework.hpp"
#include "linking_lab.hpp"

#include <cstdint>

using namespace cppbc::linking;

TEST(D8_inline, header_definition_is_callable) {
    EXPECT_EQ(inline_square(0), std::int64_t{0});
    EXPECT_EQ(inline_square(7), std::int64_t{49});
    EXPECT_EQ(inline_square(-4), std::int64_t{16});
}

TEST(D8_extern, two_translation_units_share_one_definition) {
    reset_shared_counter();
    EXPECT_EQ(increment_shared_from_alpha(), std::uint64_t{1});
    EXPECT_EQ(increment_shared_from_beta(), std::uint64_t{2});
    EXPECT_EQ(shared_counter, std::uint64_t{2});
    EXPECT_EQ(shared_address_from_alpha(), shared_address_from_beta());
}

TEST(D8_inline, static_local_in_inline_function_is_program_wide) {
    reset_inline_counter();
    EXPECT_EQ(increment_inline_from_alpha(), std::uint64_t{1});
    EXPECT_EQ(increment_inline_from_beta(), std::uint64_t{2});
    EXPECT_EQ(inline_address_from_alpha(), inline_address_from_beta());
}

TEST(D8_internal, static_header_entity_is_distinct_per_translation_unit) {
    EXPECT_NE(local_address_from_alpha(), local_address_from_beta());
    EXPECT_EQ(increment_local_from_alpha(), std::uint64_t{1});
    EXPECT_EQ(increment_local_from_beta(), std::uint64_t{1});
    EXPECT_EQ(increment_local_from_alpha(), std::uint64_t{2});
    EXPECT_EQ(increment_local_from_beta(), std::uint64_t{2});
}

TEST(D8_c_linkage, c_and_cpp_translation_units_share_one_c_api) {
    EXPECT_EQ(d8_c_add(20, 22), std::int64_t{42});
    EXPECT_EQ(d8_c_add(-5, 2), std::int64_t{-3});
    EXPECT_EQ(d8_call_add_from_c(1'500'000'000, 1'500'000'000),
              std::int64_t{3'000'000'000});
}

TEST(D8_cross_tu, composed_demo_observes_shared_and_private_state) {
    const CrossTuSnapshot snapshot = run_cross_tu_demo();
    EXPECT_EQ(snapshot.external_after_alpha, std::uint64_t{1});
    EXPECT_EQ(snapshot.external_after_beta, std::uint64_t{2});
    EXPECT_EQ(snapshot.inline_after_alpha, std::uint64_t{1});
    EXPECT_EQ(snapshot.inline_after_beta, std::uint64_t{2});
    EXPECT_EQ(snapshot.local_alpha, snapshot.local_beta); // 两份独立状态经历了相同次数的递增
    EXPECT_GT(snapshot.local_alpha, std::uint64_t{0});
    EXPECT_GT(snapshot.local_beta, std::uint64_t{0});
}
