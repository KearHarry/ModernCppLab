# 运行：ctest -R C7 --output-on-failure
add_module_test(C7_red_black_tree_test tests/test_red_black_tree.cpp)
set_tests_properties(C7_red_black_tree_test PROPERTIES TIMEOUT 30)
