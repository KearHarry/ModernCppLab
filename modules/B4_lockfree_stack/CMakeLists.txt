# 运行： ctest -R B4 --output-on-failure
add_module_test(B4_lockfree_stack_test tests/test_lockfree_stack.cpp)

# 无锁实现若有 bug 导致活锁/卡死，30 秒后自动判超时失败，避免无限重试卡住。
set_tests_properties(B4_lockfree_stack_test PROPERTIES TIMEOUT 30)
