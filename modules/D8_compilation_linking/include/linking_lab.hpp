// =============================================================================
//  D8 · 编译、链接与 ABI —— 公共头文件
// =============================================================================
//
//  本模块刻意拆成多个翻译单元：
//    - extern 声明对应一个外部链接定义，所有 .cpp 共享同一对象；
//    - inline 实体可在多个 TU 中出现相同定义，并由 ODR 规则合并；
//    - internal linkage 实体每个 TU 都有独立副本；
//    - extern "C" 指定 C 语言链接，便于稳定的跨语言符号边界。
// =============================================================================
#pragma once

#include <cstdint>
#include "d8_c_api.h"

namespace cppbc::linking {

// 声明不是定义；唯一的定义位于 shared_state.cpp。
extern std::uint64_t shared_counter;

void reset_shared_counter() noexcept;
std::uint64_t increment_shared_from_alpha() noexcept;
std::uint64_t increment_shared_from_beta() noexcept;
const std::uint64_t* shared_address_from_alpha() noexcept;
const std::uint64_t* shared_address_from_beta() noexcept;

// inline 函数可定义在头中；其函数内 static 仍是整个程序共享的一份实体。
inline std::uint64_t& inline_counter() noexcept {
    static std::uint64_t value = 0;
    return value;
}

inline std::int64_t inline_square(std::int32_t value) noexcept {
    // TODO(D8-1)：先提升到 int64_t，再返回 value * value；避免 int32_t 乘法溢出。
    (void)value;
    return 0;
}

void reset_inline_counter() noexcept;
std::uint64_t increment_inline_from_alpha() noexcept;
std::uint64_t increment_inline_from_beta() noexcept;
const std::uint64_t* inline_address_from_alpha() noexcept;
const std::uint64_t* inline_address_from_beta() noexcept;

// static 令本函数具有内部链接；头被两个 TU 包含后，各 TU 得到独立函数和独立 local。
[[maybe_unused]] static std::uint64_t& translation_unit_local_counter() noexcept {
    static std::uint64_t value = 0;
    return value;
}

std::uint64_t increment_local_from_alpha() noexcept;
std::uint64_t increment_local_from_beta() noexcept;
const std::uint64_t* local_address_from_alpha() noexcept;
const std::uint64_t* local_address_from_beta() noexcept;

struct CrossTuSnapshot {
    std::uint64_t external_after_alpha{};
    std::uint64_t external_after_beta{};
    std::uint64_t inline_after_alpha{};
    std::uint64_t inline_after_beta{};
    std::uint64_t local_alpha{};
    std::uint64_t local_beta{};
};

CrossTuSnapshot run_cross_tu_demo() noexcept;

} // namespace cppbc::linking

// C-compatible API 位于独立头 d8_c_api.h，并会被一个真正的 .c 调用方包含。
