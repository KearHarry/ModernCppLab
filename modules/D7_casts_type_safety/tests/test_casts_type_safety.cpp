// =============================================================================
//  D7 测试：所有失败路径都通过 nullptr/nullopt 表达，不故意执行错误 cast。
// =============================================================================
#include "test_framework.hpp"
#include "casts_type_safety.hpp"

#include <cstdint>
#include <limits>

using namespace cppbc::casts;

TEST(D7_dynamic_cast, classifies_polymorphic_objects) {
    ImageAsset image(20, 10);
    SoundAsset sound(48000);
    EXPECT_TRUE(classify(image) == AssetKind::image);
    EXPECT_TRUE(classify(sound) == AssetKind::sound);
}

TEST(D7_dynamic_cast, checked_downcast_succeeds_or_returns_null) {
    ImageAsset image(8, 4);
    SoundAsset sound(32);
    const Asset* image_base = &image;
    const Asset* sound_base = &sound;

    ASSERT_TRUE(try_image(image_base) != nullptr);
    const auto pixels = try_image(image_base)->pixels();
    ASSERT_TRUE(pixels.has_value());
    EXPECT_EQ(*pixels, 32);
    EXPECT_TRUE(try_image(sound_base) == nullptr);
    EXPECT_TRUE(try_image(nullptr) == nullptr);
}

TEST(D7_numeric_safety, pixel_count_rejects_negative_or_overflowing_dimensions) {
    const ImageAsset negative(-1, 4);
    const ImageAsset overflowing(std::numeric_limits<int>::max(), 2);
    EXPECT_FALSE(negative.pixels().has_value());
    EXPECT_FALSE(overflowing.pixels().has_value());
}

TEST(D7_static_cast, checked_narrowing_accepts_boundaries) {
    const auto low = checked_to_int(std::numeric_limits<int>::min());
    const auto high = checked_to_int(std::numeric_limits<int>::max());
    ASSERT_TRUE(low.has_value());
    ASSERT_TRUE(high.has_value());
    EXPECT_EQ(*low, std::numeric_limits<int>::min());
    EXPECT_EQ(*high, std::numeric_limits<int>::max());
}

TEST(D7_static_cast, checked_narrowing_rejects_out_of_range) {
    const long long int_low = static_cast<long long>(std::numeric_limits<int>::min());
    const long long int_high = static_cast<long long>(std::numeric_limits<int>::max());

    bool rejects_below = (std::numeric_limits<long long>::min() == int_low);
    if (!rejects_below) {
        long long below = int_low;
        --below;
        rejects_below = !checked_to_int(below).has_value();
    }

    bool rejects_above = (std::numeric_limits<long long>::max() == int_high);
    if (!rejects_above) {
        long long above = int_high;
        ++above;
        rejects_above = !checked_to_int(above).has_value();
    }

    // 若 long long 与 int 某一侧范围相同，该侧不存在可构造的越界值；这也不是失败。
    EXPECT_TRUE(rejects_below);
    EXPECT_TRUE(rejects_above);
}

TEST(D7_const_cast, mutation_is_safe_when_underlying_object_is_mutable) {
    int value = 41;
    EXPECT_EQ(increment_through_const_view(value), 42);
    EXPECT_EQ(value, 42);
}

TEST(D7_reinterpret_cast, integer_round_trip_preserves_pointer_identity) {
    int value = 7;
    EXPECT_TRUE(pointer_integer_round_trip(&value));
    EXPECT_TRUE(pointer_integer_round_trip(nullptr));
}

TEST(D7_representation, little_endian_decoder_is_alignment_independent) {
    const std::array<std::byte, 4> bytes{
        std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12}};
    EXPECT_EQ(decode_u32_le(bytes), std::uint32_t{0x12345678});
}

TEST(D7_representation, byte_views_preserve_float_object_representation) {
    const float value = 1.0F;
    const auto bytes = object_bytes(value);
    EXPECT_EQ(bytes.size(), sizeof(float));
    EXPECT_EQ(float_bytes(value), bytes);
}
