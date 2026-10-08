# 运行：ctest -R B6 --output-on-failure
add_module_test(B6_thread_lifecycle_test tests/test_thread_lifecycle.cpp)

# 生命周期/等待写错时不能让整套课程永久卡住。
set_tests_properties(B6_thread_lifecycle_test PROPERTIES TIMEOUT 30)
