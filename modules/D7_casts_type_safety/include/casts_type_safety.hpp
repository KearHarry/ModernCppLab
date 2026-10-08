// =============================================================================
//  D7 · 类型转换与类型安全 —— 新手导读
// =============================================================================
//
//  四种命名 cast 不是“四种写法”，而是四种不同承诺：
//    static_cast      编译期已知关系或显式数值转换；不做运行期类型检查。
//    dynamic_cast     在多态继承图中做运行期检查；失败时指针为空、引用抛异常。
//    const_cast       只改变 cv 限定；修改一个真正 const 的对象仍是未定义行为。
//    reinterpret_cast 解释地址/位表示等低层关系；不能绕过别名、对齐和生命周期规则。
//
//  本实验把危险边界包在窄小、可测试的函数中。没有任何测试会故意触发 UB。
// =============================================================================
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace cppbc::casts {

class Asset {
public:
    virtual ~Asset() = default;
    virtual std::string_view name() const noexcept = 0;
};

class ImageAsset final : public Asset {
    int width_;
    int height_;

public:
    ImageAsset(int width, int height) : width_(width), height_(height) {}
    std::string_view name() const noexcept override { return "image"; }
    // 负尺寸或乘积超出 int 时返回空，避免用有符号溢出来表达“像素过多”。
    std::optional<int> pixels() const noexcept {
        if (width_ < 0 || height_ < 0) return std::nullopt;
        if (height_ != 0 && width_ > std::numeric_limits<int>::max() / height_) {
            return std::nullopt;
        }
        return width_ * height_;
    }
};

class SoundAsset final : public Asset {
    int samples_;

public:
    explicit SoundAsset(int samples) : samples_(samples) {}
    std::string_view name() const noexcept override { return "sound"; }
    int samples() const noexcept { return samples_; }
};

enum class AssetKind { unknown, image, sound };

inline AssetKind classify(const Asset& asset) noexcept {
    // TODO(D7-1)：分别 dynamic_cast<const ImageAsset*> 和 SoundAsset*；根据结果返回。
    (void)asset;
    return AssetKind::unknown;
}

inline const ImageAsset* try_image(const Asset* asset) noexcept {
    // TODO(D7-2)：用 dynamic_cast 做受检向下转换；asset==nullptr 时自然返回 nullptr。
    (void)asset;
    return nullptr;
}

// static_cast 做数值收窄不会替你检查范围。因此先验证，再显式转换。
inline std::optional<int> checked_to_int(long long value) noexcept {
    // TODO(D7-3)：若超出 int 的 min/max 返回 nullopt，否则 static_cast<int>(value)。
    (void)value;
    return std::nullopt;
}

// const_cast 的安全用法：底层对象明确是非 const，只是经 const 视图传递。
// 如果把真正的 const int 传进来并修改才是 UB，所以接口刻意接收 int&，从类型上保留事实。
inline int increment_through_const_view(int& value) noexcept {
    // TODO(D7-4)：创建 const int& view=value；const_cast<int&>(view) 后自增并返回。
    return value;
}

// 指针 -> uintptr_t -> 同类型指针的往返在 uintptr_t 存在且足够容纳指针的平台可恢复原值。
// 本函数只比较，不解引用伪造地址。
inline bool pointer_integer_round_trip(void* pointer) noexcept {
    // TODO(D7-5)：reinterpret_cast 为 uintptr_t，再转回 void*，比较是否相同。
    (void)pointer;
    return false;
}

// 从可平凡复制对象读取其“对象表示”时，unsigned char/std::byte 是别名规则特许的安全视图。
template <class T>
requires std::is_trivially_copyable_v<T>
std::array<std::byte, sizeof(T)> object_bytes(const T& object) noexcept {
    std::array<std::byte, sizeof(T)> result{};
    const auto* first = reinterpret_cast<const unsigned char*>(&object);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        result[i] = static_cast<std::byte>(first[i]);
    }
    return result;
}

// 协议字节流可能未对齐，也不能当 uint32_t* 解引用。逐字节解码最清楚、无别名 UB。
inline std::uint32_t decode_u32_le(const std::array<std::byte, 4>& bytes) noexcept {
    // TODO(D7-6)：把四个字节转 uint32_t，分别左移 0/8/16/24 后按位或。
    (void)bytes;
    return 0;
}

inline std::array<std::byte, sizeof(float)> float_bytes(float value) noexcept {
    // C++ 不保证 float 恰为 32 位；按 sizeof(float) 返回字节表示才能保持可移植。
    return object_bytes(value);
}

} // namespace cppbc::casts
