#include "linking_lab.hpp"

namespace cppbc::linking {

std::uint64_t increment_shared_from_beta() noexcept {
    // TODO(D8-2)：与 alpha.cpp 操作同一个 extern 对象。
    return shared_counter;
}

const std::uint64_t* shared_address_from_beta() noexcept { return &shared_counter; }

std::uint64_t increment_inline_from_beta() noexcept {
    // TODO(D8-3)：与 alpha.cpp 操作同一个 inline static local。
    return inline_counter();
}

const std::uint64_t* inline_address_from_beta() noexcept { return &inline_counter(); }

std::uint64_t increment_local_from_beta() noexcept {
    // TODO(D8-4)：操作 beta TU 自己的内部链接对象。
    return translation_unit_local_counter();
}

const std::uint64_t* local_address_from_beta() noexcept {
    return &translation_unit_local_counter();
}

} // namespace cppbc::linking
