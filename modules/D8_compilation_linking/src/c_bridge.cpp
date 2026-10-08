#include "d8_c_api.h"

extern "C" int64_t d8_c_add(int32_t left, int32_t right) {
    // TODO(D8-5)：提升后相加。实现仍是 C++，但导出函数使用 C language linkage。
    (void)left;
    (void)right;
    return 0;
}
