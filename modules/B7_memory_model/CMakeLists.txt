# 运行：ctest -R B7 --output-on-failure
add_module_test(B7_memory_model_test tests/test_memory_model.cpp)
set_tests_properties(B7_memory_model_test PROPERTIES TIMEOUT 30)
