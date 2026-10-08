#include "linking_lab.hpp"

namespace cppbc::linking {

std::uint64_t shared_counter = 0; // 全程序唯一的外部链接定义；无符号回绕有定义。

void reset_shared_counter() noexcept { shared_counter = 0; }
void reset_inline_counter() noexcept { inline_counter() = 0; }

} // namespace cppbc::linking
