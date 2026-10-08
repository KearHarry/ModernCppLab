#ifndef MODERN_CPP_LAB_D8_C_API_H
#define MODERN_CPP_LAB_D8_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 32 位输入的和一定可由 int64_t 表示，避免 C/C++ 两侧的有符号溢出。
int64_t d8_c_add(int32_t left, int32_t right);

// 定义在 c_caller.c：测试借它确认该头确实可由 C 编译器包含和调用。
int64_t d8_call_add_from_c(int32_t left, int32_t right);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MODERN_CPP_LAB_D8_C_API_H
