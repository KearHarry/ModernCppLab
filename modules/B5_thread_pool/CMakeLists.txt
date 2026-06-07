# 运行： ctest -R B5 --output-on-failure
add_module_test(B5_thread_pool_test tests/test_thread_pool.cpp)

# 并发测试若因 bug（如 worker_loop 退出条件写错、future 永远等不到）死锁，
# 30 秒后自动判超时失败，避免无限卡住。
set_tests_properties(B5_thread_pool_test PROPERTIES TIMEOUT 30)
