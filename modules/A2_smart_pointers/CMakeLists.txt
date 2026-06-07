# 把 tests/test_smart_ptr.cpp 编译成 A2_smart_pointers_test 可执行文件并注册到 CTest。
# 运行： ctest -R A2 --output-on-failure
add_module_test(A2_smart_pointers_test tests/test_smart_ptr.cpp)
