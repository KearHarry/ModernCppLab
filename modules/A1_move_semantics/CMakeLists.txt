# 【新手】这一行告诉 CMake：
#   把 tests/test_move_semantics.cpp 编译成可执行文件 A1_move_semantics_test.exe
#   并注册到 CTest，之后可以用 ctest -R A1 运行。
# 真正的编译规则在仓库根目录 CMakeLists.txt 的 add_module_test 函数里。
add_module_test(A1_move_semantics_test tests/test_move_semantics.cpp)
