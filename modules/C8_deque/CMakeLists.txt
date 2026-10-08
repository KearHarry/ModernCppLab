# 运行：ctest -R C8 --output-on-failure
add_module_test(C8_deque_test tests/test_segmented_deque.cpp)
set_tests_properties(C8_deque_test PROPERTIES TIMEOUT 30)
