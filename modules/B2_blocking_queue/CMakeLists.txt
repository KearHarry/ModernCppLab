# 运行： ctest -R B2 --output-on-failure
add_module_test(B2_blocking_queue_test tests/test_blocking_queue.cpp)

# 并发测试若因 bug 死锁，30 秒后自动判超时失败（避免无限卡住）。
set_tests_properties(B2_blocking_queue_test PROPERTIES TIMEOUT 30)
