#include "linking_lab.hpp"

namespace cppbc::linking {

std::uint64_t increment_shared_from_alpha() noexcept {
    // TODO(D8-2)：前置递增 shared_counter 并返回。
    return shared_counter;
}

const std::uint64_t* shared_address_from_alpha() noexcept { return &shared_counter; }

std::uint64_t increment_inline_from_alpha() noexcept {
    // TODO(D8-3)：前置递增 inline_counter() 并返回。
    return inline_counter();
}

const std::uint64_t* inline_address_from_alpha() noexcept { return &inline_counter(); }

std::uint64_t increment_local_from_alpha() noexcept {
    // TODO(D8-4)：前置递增 translation_unit_local_counter() 并返回。
    return translation_unit_local_counter();
}

const std::uint64_t* local_address_from_alpha() noexcept {
    return &translation_unit_local_counter();
}

} // namespace cppbc::linking
