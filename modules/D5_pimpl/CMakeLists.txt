# 运行： ctest -R D5 --output-on-failure
#   注意：实现放在 src/widget.cpp，需与测试一起编译（add_module_test 接收多个源文件）。
add_module_test(D5_pimpl_test tests/test_widget.cpp src/widget.cpp)
