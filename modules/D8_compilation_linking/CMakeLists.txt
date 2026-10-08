# 多翻译单元是本模块的核心；缺少任一源文件都会得到未定义引用。
# 运行：ctest -R D8 --output-on-failure
add_module_test(D8_compilation_linking_test
    tests/test_compilation_linking.cpp
    src/shared_state.cpp
    src/alpha.cpp
    src/beta.cpp
    src/c_bridge.cpp
    src/c_caller.c
    src/linking_lab.cpp)
