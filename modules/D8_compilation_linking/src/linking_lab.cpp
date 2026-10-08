#include "linking_lab.hpp"

namespace cppbc::linking {

CrossTuSnapshot run_cross_tu_demo() noexcept {
    // TODO(D8-6)：先 reset 两个共享计数器，再依次调用 alpha/beta 的三个 increment，
    //               将六个返回值填入快照。local 不需 reset：本测试只调用本函数一次。
    return {};
}

} // namespace cppbc::linking
