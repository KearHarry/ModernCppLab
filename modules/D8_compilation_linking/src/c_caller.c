#include "d8_c_api.h"

int64_t d8_call_add_from_c(int32_t left, int32_t right) {
    return d8_c_add(left, right);
}
