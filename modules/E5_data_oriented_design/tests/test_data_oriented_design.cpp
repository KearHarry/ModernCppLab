// =============================================================================
//  E5 测试：验证值语义等价、地址步长和确定性工作量；绝不按运行时间判成败。
// =============================================================================
#include "test_framework.hpp"
#include "data_oriented_design.hpp"

#include <cmath>
#include <cstddef>

using namespace cppbc::dod;

static bool close(float a, float b) { return std::fabs(a - b) < 1e-6F; }

static Particle make_particle(float base, std::uint32_t flags = 0) {
    return Particle{base, base + 1, base + 2,
                    base + 3, base + 4, base + 5, flags};
}

TEST(E5_storage, aos_and_soa_preserve_the_same_values) {
    AoSParticles aos;
    SoAParticles soa;
    aos.push_back(make_particle(1.0F, 7));
    soa.push_back(make_particle(1.0F, 7));
    ASSERT_EQ(aos.size(), 1u);
    ASSERT_EQ(soa.size(), 1u);

    EXPECT_TRUE(close(aos[0].x, soa.x(0)));
    EXPECT_TRUE(close(aos[0].vy, soa.vy(0)));
    EXPECT_EQ(aos[0].flags, soa.flags(0));
}

TEST(E5_reserve, allocation_events_reflect_independent_buffers) {
    AoSParticles aos;
    SoAParticles soa;
    aos.reserve(100);
    soa.reserve(100);

    EXPECT_EQ(aos.allocation_events(), 1u);
    EXPECT_EQ(soa.allocation_events(), SoAParticles::kColumnCount);
    EXPECT_GE(aos.capacity(), 100u);
    EXPECT_GE(soa.capacity(), 100u);

    const auto aos_events = aos.allocation_events();
    const auto soa_events = soa.allocation_events();
    const auto soa_capacities = soa.column_capacities();
    aos.reserve(50); // 较小 reserve 不应重新分配或重复记账
    soa.reserve(50);
    EXPECT_EQ(aos.allocation_events(), aos_events);
    EXPECT_EQ(soa.allocation_events(), soa_events);
    EXPECT_TRUE(soa.column_capacities() == soa_capacities);
}

TEST(E5_reserve, reserved_pushes_cause_no_more_growth_events) {
    AoSParticles aos;
    SoAParticles soa;
    aos.reserve(4);
    soa.reserve(4);
    for (int i = 0; i < 4; ++i) {
        aos.push_back(make_particle(static_cast<float>(i)));
        soa.push_back(make_particle(static_cast<float>(i)));
    }
    EXPECT_EQ(aos.allocation_events(), 1u);
    EXPECT_EQ(soa.allocation_events(), SoAParticles::kColumnCount);
}

TEST(E5_growth, unreserved_pushes_record_each_actual_buffer_growth) {
    AoSParticles aos;
    SoAParticles soa;
    std::size_t expected_aos_events = 0;
    std::size_t expected_soa_events = 0;

    for (int i = 0; i < 16; ++i) {
        const auto aos_before = aos.capacity();
        const auto soa_before = soa.column_capacities();
        const Particle p = make_particle(static_cast<float>(i));
        aos.push_back(p);
        soa.push_back(p);
        if (aos.capacity() > aos_before) ++expected_aos_events;
        const auto soa_after = soa.column_capacities();
        for (std::size_t column = 0; column < soa_after.size(); ++column) {
            if (soa_after[column] > soa_before[column]) ++expected_soa_events;
        }
    }

    ASSERT_EQ(aos.size(), 16u);
    ASSERT_EQ(soa.size(), 16u);
    EXPECT_GT(expected_aos_events, 0u);
    EXPECT_GT(expected_soa_events, 0u);
    EXPECT_EQ(aos.allocation_events(), expected_aos_events);
    EXPECT_EQ(soa.allocation_events(), expected_soa_events);
}

TEST(E5_layout, neighboring_hot_values_have_different_strides) {
    AoSParticles aos;
    SoAParticles soa;
    aos.reserve(2);
    soa.reserve(2);
    aos.push_back(make_particle(1.0F));
    aos.push_back(make_particle(2.0F));
    soa.push_back(make_particle(1.0F));
    soa.push_back(make_particle(2.0F));
    ASSERT_EQ(aos.size(), 2u);
    ASSERT_EQ(soa.size(), 2u);

    const auto aos0 = reinterpret_cast<std::uintptr_t>(&aos[0].x);
    const auto aos1 = reinterpret_cast<std::uintptr_t>(&aos[1].x);
    const auto soa0 = reinterpret_cast<std::uintptr_t>(&soa.x_data()[0]);
    const auto soa1 = reinterpret_cast<std::uintptr_t>(&soa.x_data()[1]);
    EXPECT_EQ(aos1 - aos0, static_cast<std::uintptr_t>(sizeof(Particle)));
    EXPECT_EQ(soa1 - soa0, static_cast<std::uintptr_t>(sizeof(float)));
}

TEST(E5_update, aos_and_soa_produce_identical_positions) {
    AoSParticles aos;
    SoAParticles soa;
    for (int i = 0; i < 3; ++i) {
        const Particle p = make_particle(static_cast<float>(i + 1));
        aos.push_back(p);
        soa.push_back(p);
    }
    WorkStats aos_stats;
    WorkStats soa_stats;
    advance(aos, 0.5F, aos_stats);
    advance(soa, 0.5F, soa_stats);

    ASSERT_EQ(aos.size(), soa.size());
    for (std::size_t i = 0; i < aos.size(); ++i) {
        const float base = static_cast<float>(i + 1);
        const float expected_x = base + (base + 3.0F) * 0.5F;
        const float expected_y = base + 1.0F + (base + 4.0F) * 0.5F;
        const float expected_z = base + 2.0F + (base + 5.0F) * 0.5F;
        EXPECT_TRUE(close(aos[i].x, expected_x));
        EXPECT_TRUE(close(aos[i].y, expected_y));
        EXPECT_TRUE(close(aos[i].z, expected_z));
        EXPECT_TRUE(close(soa.x(i), expected_x));
        EXPECT_TRUE(close(soa.y(i), expected_y));
        EXPECT_TRUE(close(soa.z(i), expected_z));
        EXPECT_TRUE(close(aos[i].x, soa.x(i)));
        EXPECT_TRUE(close(aos[i].y, soa.y(i)));
        EXPECT_TRUE(close(aos[i].z, soa.z(i)));
    }
    EXPECT_EQ(aos_stats.visited, 3u);
    EXPECT_EQ(soa_stats.visited, 3u);
    EXPECT_EQ(aos_stats.scalar_reads, 18u);
    EXPECT_EQ(soa_stats.scalar_reads, 18u);
    EXPECT_EQ(aos_stats.scalar_writes, 9u);
    EXPECT_EQ(soa_stats.scalar_writes, 9u);
    EXPECT_EQ(aos_stats.modeled_bytes, 3u * sizeof(Particle));
    EXPECT_EQ(soa_stats.modeled_bytes, 3u * 6u * sizeof(float));
}

TEST(E5_hot_scan, soa_has_smaller_deterministic_modeled_working_set) {
    AoSParticles aos;
    SoAParticles soa;
    for (int i = 0; i < 8; ++i) {
        const Particle p = make_particle(static_cast<float>(i));
        aos.push_back(p);
        soa.push_back(p);
    }
    WorkStats aos_stats;
    WorkStats soa_stats;
    const float aos_sum = sum_x(aos, aos_stats);
    const float soa_sum = sum_x(soa, soa_stats);

    EXPECT_TRUE(close(aos_sum, 28.0F));
    EXPECT_TRUE(close(soa_sum, 28.0F));
    EXPECT_EQ(aos_stats.visited, 8u);
    EXPECT_EQ(soa_stats.visited, 8u);
    EXPECT_EQ(aos_stats.scalar_reads, 8u);
    EXPECT_EQ(soa_stats.scalar_reads, 8u);
    EXPECT_EQ(aos_stats.scalar_writes, 0u);
    EXPECT_EQ(soa_stats.scalar_writes, 0u);
    EXPECT_EQ(aos_stats.modeled_bytes, 8u * sizeof(Particle));
    EXPECT_EQ(soa_stats.modeled_bytes, 8u * sizeof(float));
    EXPECT_LT(soa_stats.modeled_bytes, aos_stats.modeled_bytes);
}

TEST(E5_empty, algorithms_are_well_defined_for_empty_storage) {
    AoSParticles aos;
    SoAParticles soa;
    WorkStats a;
    WorkStats s;
    advance(aos, 1.0F, a);
    advance(soa, 1.0F, s);
    EXPECT_TRUE(close(sum_x(aos, a), 0.0F));
    EXPECT_TRUE(close(sum_x(soa, s), 0.0F));
    EXPECT_EQ(a.visited, 0u);
    EXPECT_EQ(s.visited, 0u);
}
