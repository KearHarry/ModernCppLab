// =============================================================================
//  D6 测试：只观察标准保证的顺序、大小/对齐关系与安全的临时生命周期。
// =============================================================================
#include "test_framework.hpp"
#include "lifetime_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace cppbc::lifetime_layout;

TEST(D6_lifetime, construction_follows_base_then_declaration_order) {
    EventLog log;
    {
        LifetimeObject object(log);
        const std::vector<std::string> expected{
            "base ctor", "first ctor", "second ctor", "derived body"};
        EXPECT_EQ(log.events(), expected);
    }
}

TEST(D6_lifetime, destruction_reverses_construction_order) {
    EventLog log;
    { LifetimeObject object(log); }

    const std::vector<std::string> expected{
        "base ctor", "first ctor", "second ctor", "derived body",
        "derived dtor", "second dtor", "first dtor", "base dtor"};
    EXPECT_EQ(log.events(), expected);
}

TEST(D6_temporary, const_reference_extends_local_temporary_lifetime) {
    EXPECT_TRUE(temporary_survives_local_const_reference());
}

TEST(D6_alignment, align_up_handles_aligned_and_unaligned_values) {
    EXPECT_EQ(align_up(0u, 8u), static_cast<std::uintptr_t>(0u));
    EXPECT_EQ(align_up(1u, 8u), static_cast<std::uintptr_t>(8u));
    EXPECT_EQ(align_up(8u, 8u), static_cast<std::uintptr_t>(8u));
    EXPECT_EQ(align_up(9u, 8u), static_cast<std::uintptr_t>(16u));
    EXPECT_EQ(align_up(65u, 64u), static_cast<std::uintptr_t>(128u));
}

TEST(D6_alignment, alignas_storage_satisfies_requested_type_alignment) {
    // 只使用标准保证支持的基本对齐，避免把 alignas(64) 这类扩展对齐当成跨平台保证。
    alignas(double) std::byte storage[sizeof(double)]{};
    alignas(std::max_align_t) std::byte max_storage[sizeof(std::max_align_t)]{};
    EXPECT_TRUE(is_aligned_for<double>(storage));
    EXPECT_TRUE(is_aligned_for<std::max_align_t>(max_storage));
}

TEST(D6_layout, report_uses_current_compiler_abi_not_magic_numbers) {
    const LayoutReport report = inspect_layout();
    EXPECT_EQ(report.poor_size, sizeof(PoorLayout));
    EXPECT_EQ(report.compact_size, sizeof(CompactLayout));
    EXPECT_EQ(report.poor_alignment, alignof(PoorLayout));
    EXPECT_EQ(report.compact_alignment, alignof(CompactLayout));
    EXPECT_EQ(report.plain_policy_size, sizeof(PlainPolicyHolder));
    EXPECT_EQ(report.ebo_policy_size, sizeof(EboPolicyHolder));
    EXPECT_EQ(report.attribute_policy_size, sizeof(AttributePolicyHolder));
}

TEST(D6_layout, sizes_cover_payload_without_requiring_optional_abi_optimizations) {
    // 成员重排、EBO 和 no_unique_address 通常能省空间，但标准不保证这些 <= 关系。
    // 这里只验证对象足以容纳非重叠的普通有效载荷；具体 ABI 数值由 LayoutReport 展示。
    EXPECT_GE(sizeof(PoorLayout), sizeof(char) * 2 + sizeof(double));
    EXPECT_GE(sizeof(CompactLayout), sizeof(char) * 2 + sizeof(double));
    EXPECT_GE(sizeof(PlainPolicyHolder), sizeof(int));
    EXPECT_GE(sizeof(EboPolicyHolder), sizeof(int));
    EXPECT_GE(sizeof(AttributePolicyHolder), sizeof(int));
}

TEST(D6_layout, member_offsets_follow_declaration_order) {
    EXPECT_LT(offsetof(PoorLayout, first), offsetof(PoorLayout, value));
    EXPECT_LT(offsetof(PoorLayout, value), offsetof(PoorLayout, last));
    EXPECT_LT(offsetof(CompactLayout, value), offsetof(CompactLayout, first));
    EXPECT_LT(offsetof(CompactLayout, first), offsetof(CompactLayout, last));
}
