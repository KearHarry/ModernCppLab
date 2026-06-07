# 运行： ctest -R B3 --output-on-failure
add_module_test(B3_atomics_memory_order_test tests/test_atomics.cpp)

# 并发测试若因 bug（如忘了 unlock）死锁，30 秒后自动判超时失败，避免无限卡住。
set_tests_properties(B3_atomics_memory_order_test PROPERTIES TIMEOUT 30)
