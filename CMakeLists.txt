# =============================================================================
#  ModernCppLab · C++ 大厂面试特训营 —— 顶层构建脚本
# -----------------------------------------------------------------------------
#  常用命令（在仓库根目录执行）:
#      cmake -S . -B build -G "MinGW Makefiles"
#      cmake --build build -j
#      ctest --test-dir build --output-on-failure      # 跑全部模块测试
#      ctest --test-dir build -R '^A1_' --output-on-failure # 只跑某个模块
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(ModernCppLab LANGUAGES C CXX)

# ---- 语言标准：C++20，强制要求，禁用编译器扩展（保证可移植写法） -----------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# D8 含一个真正由 C 编译器编译的调用方，用来验证 C-compatible 边界。
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 未指定构建类型时默认 Debug（带调试符号，方便配合 gdb / 断言定位）。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
endif()

# 线程库（并发模块需要 std::thread / std::mutex）。
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

enable_testing()

# -----------------------------------------------------------------------------
#  add_module_test(<target> <源文件...>)
#    在「当前模块目录」下创建一个测试可执行文件，并注册到 CTest。
#    自动加入头文件搜索路径：模块目录、模块/include、顶层 common（测试框架）。
#    自动链接线程库。各模块的 CMakeLists.txt 调用它即可，无需重复样板。
# -----------------------------------------------------------------------------
function(add_module_test target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/common)
    target_link_libraries(${target} PRIVATE Threads::Threads)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    endif()

    add_test(NAME ${target} COMMAND ${target})
    # 学员实现并发/无锁结构时可能误写出死循环；让所有测试入口都有有限等待。
    set_tests_properties(${target} PROPERTIES TIMEOUT 30)
endfunction()

# -----------------------------------------------------------------------------
#  课程模块。命名规则：<Track><序号>_<主题>
#    A 现代C++核心特性 | B 并发编程 | C STL底层实现 | D 经典工程题与对象模型 | E 引擎/游戏常用设施
# -----------------------------------------------------------------------------
# 现代 C++ 核心特性
add_subdirectory(modules/A1_move_semantics)
add_subdirectory(modules/A2_smart_pointers)
add_subdirectory(modules/A3_template_metaprogramming)
add_subdirectory(modules/A4_type_erasure)
add_subdirectory(modules/A5_optional)
add_subdirectory(modules/A6_coroutine_generator)
add_subdirectory(modules/A7_type_deduction)
add_subdirectory(modules/A8_variant_visit)
add_subdirectory(modules/A9_exception_safety)
add_subdirectory(modules/A10_lambda_invoke)

# 并发编程
add_subdirectory(modules/B1_rwlock)
add_subdirectory(modules/B2_blocking_queue)
add_subdirectory(modules/B3_atomics_memory_order)
add_subdirectory(modules/B4_lockfree_stack)
add_subdirectory(modules/B5_thread_pool)
add_subdirectory(modules/B6_thread_lifecycle)
add_subdirectory(modules/B7_memory_model)

# STL 底层实现
add_subdirectory(modules/C1_vector)
add_subdirectory(modules/C2_string_sso)
add_subdirectory(modules/C3_memory_pool)
add_subdirectory(modules/C4_hash_table)
add_subdirectory(modules/C5_intrusive_list)
add_subdirectory(modules/C6_skip_list)
add_subdirectory(modules/C7_red_black_tree)
add_subdirectory(modules/C8_deque)
add_subdirectory(modules/C9_allocator_pmr)
add_subdirectory(modules/C10_iterators_ranges)

# 经典工程题与对象模型
add_subdirectory(modules/D1_lru_cache)
add_subdirectory(modules/D2_crtp)
add_subdirectory(modules/D3_singleton)
add_subdirectory(modules/D4_object_model)
add_subdirectory(modules/D5_pimpl)
add_subdirectory(modules/D6_lifetime_layout)
add_subdirectory(modules/D7_casts_type_safety)
add_subdirectory(modules/D8_compilation_linking)

# 引擎 / 游戏常用设施
add_subdirectory(modules/E1_slot_map)
add_subdirectory(modules/E2_arena_allocator)
add_subdirectory(modules/E3_delegate)
add_subdirectory(modules/E4_string_id)
add_subdirectory(modules/E5_data_oriented_design)
add_subdirectory(modules/E6_sparse_set_ecs)
