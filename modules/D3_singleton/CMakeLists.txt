# 运行： ctest -R D3 --output-on-failure
add_module_test(D3_singleton_test tests/test_singleton.cpp)

# 并发测试若因 bug（如同步写错导致死锁）卡住，30 秒后自动判超时失败。
set_tests_properties(D3_singleton_test PROPERTIES TIMEOUT 30)
