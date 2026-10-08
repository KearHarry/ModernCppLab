// =============================================================================
//  test_framework.hpp  --  零依赖、header-only 的极简测试框架
// -----------------------------------------------------------------------------
//
//  【新手：这个框架在整个仓库里扮演什么角色？】
//
//  你可以把这个仓库想成"很多道编程题"：
//    - 每道题在 modules/某文件夹/ 里
//    - 题目代码在 include/xxx.hpp（你填空）
//    - 判题程序在 tests/test_xxx.cpp（自动检查你对不对）
//    - 本文件 common/test_framework.hpp 是"判题引擎"（不用改）
//
//  【你平时只需要关心两件事】
//    1. 改 include/ 里的 TODO
//    2. 编译并运行 tests/ 对应的 .exe，看红变绿
//
//  【#include "test_framework.hpp" 之后发生了什么？】
//    - 你可以写 TEST(名字, 子名字) { ... } 定义测试
//    - 文件末尾会自动有一个 main()，启动时跑完所有 TEST
//    - EXPECT_EQ 等宏失败时会打印红色 [FAIL] 和行号
//
//  【和 LeetCode 的对比（帮助理解）】
//    - LeetCode：你写 solution，平台跑 hidden tests
//    - 本仓库：你写 move_semantics.hpp，test_xxx.cpp 是公开的 tests
//
// -----------------------------------------------------------------------------
//  设计目标:
//    1. 开箱即用：每个测试可执行文件只需 #include 本文件，main() 由本文件提供。
//    2. 风格贴近 GoogleTest：TEST(suite, name) + EXPECT_*/ASSERT_* 宏。
//    3. 失败信息带 文件:行号 与实际值，方便定位。
//
//  用法：
//      #include "test_framework.hpp"
//      #include "your_module.hpp"
//
//      TEST(MySuite, does_something) {
//          EXPECT_EQ(1 + 1, 2);
//          ASSERT_TRUE(ptr != nullptr);   // ASSERT 失败会中止当前用例
//          EXPECT_THROW(foo(), std::runtime_error);
//      }
//      // 无需手写 main()，本框架自动生成。
//
//  约定：
//    * 一个测试可执行文件 = 一个 .cpp（它 include 本头文件，得到唯一的 main）。
//      模块自身的 .cpp 源文件【不要】include 本头文件，否则会有重复 main。
// =============================================================================
#pragma once

// 在包含 <windows.h> 之前定义这些宏，避免 min/max 宏污染、减少头文件体积。
#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <exception>
#include <ostream>

namespace tf {

// ---- ASSERT_* 失败时抛出，用于中止（unwind）当前测试用例 --------------------
struct AssertionFailure {};

// ---- 一个测试用例：suite 名、用例名、函数指针 ------------------------------
struct TestCase {
    const char* suite;
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;   // 函数内 static：保证初始化顺序安全
    return r;
}

// 借助全局对象的构造函数，在 main 之前完成「自注册」。
struct Registrar {
    Registrar(const char* suite, const char* name, void (*fn)()) {
        registry().push_back(TestCase{suite, name, fn});
    }
};

// ---- 当前用例的失败计数（用例间串行执行，全局变量即可） --------------------
inline int& current_failures() { static int n = 0; return n; }
inline int& total_checks()     { static int n = 0; return n; }

// ---- C++20 concept：检测某类型是否可被 operator<< 输出 ----------------------
// 让 EXPECT_EQ 能打印「实际值」，对不可打印的类型则回退为 "<?>"。
template <class T>
concept Streamable = requires(std::ostream& os, const T& v) { os << v; };

template <class T>
std::string repr(const T& v) {
    if constexpr (Streamable<T>) {
        std::ostringstream o;
        o << v;
        return o.str();
    } else {
        return "<?>";
    }
}

// ---- ANSI 颜色（Windows 下需开启虚拟终端处理） -----------------------------
namespace color {
    inline const char* red()   { return "\x1b[31m"; }
    inline const char* green() { return "\x1b[32m"; }
    inline const char* dim()   { return "\x1b[90m"; }
    inline const char* bold()  { return "\x1b[1m";  }
    inline const char* reset() { return "\x1b[0m";  }
}

inline void enable_console_utf8() {
#ifdef _WIN32
    // 源码里的中文是 UTF-8；Windows 控制台默认 GBK，不切换就会显示成乱码。
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

inline void enable_ansi_colors() {
#ifdef _WIN32
    enable_console_utf8();
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

inline void report_failure(const char* file, int line, const std::string& msg) {
    if (file && file[0]) {
        std::printf("    %s[FAIL]%s %s:%d\n           %s\n",
                    color::red(), color::reset(), file, line, msg.c_str());
    } else {
        std::printf("    %s[FAIL]%s %s\n", color::red(), color::reset(), msg.c_str());
    }
}

} // namespace tf

// =============================================================================
//  宏定义
// =============================================================================
#define TF_CONCAT_INNER(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT_INNER(a, b)

// TEST(suite, name) { ... }
#define TEST(suite, name)                                                       \
    static void TF_CONCAT(tf_fn_, TF_CONCAT(suite, TF_CONCAT(_, name)))();       \
    static ::tf::Registrar TF_CONCAT(tf_reg_, TF_CONCAT(suite, TF_CONCAT(_, name)))( \
        #suite, #name, &TF_CONCAT(tf_fn_, TF_CONCAT(suite, TF_CONCAT(_, name)))); \
    static void TF_CONCAT(tf_fn_, TF_CONCAT(suite, TF_CONCAT(_, name)))()

// 统一的检查宏：cond 为真则通过；为假则记录失败；fatal 时抛异常中止用例。
#define TF_CHECK(cond, fatal, message)                                          \
    do {                                                                        \
        ++::tf::total_checks();                                                  \
        if (!(cond)) {                                                          \
            ++::tf::current_failures();                                          \
            std::ostringstream tf_oss_;                                          \
            tf_oss_ << message;                                                  \
            ::tf::report_failure(__FILE__, __LINE__, tf_oss_.str());             \
            if (fatal) throw ::tf::AssertionFailure{};                           \
        }                                                                        \
    } while (0)

// 二元比较要把两侧表达式各求值一次并保存结果。旧版宏在失败消息中再次展开
// a/b；若参数是 counter()、pop() 之类带副作用的调用，一次失败会悄悄执行
// 第二遍，继而污染后续断言。这里先绑定局部引用，再比较和打印同一份结果。
#define TF_BINARY_CHECK(a, b, op, fatal, macro_name, failure_relation)          \
    do {                                                                        \
        ++::tf::total_checks();                                                  \
        auto&& tf_binary_lhs_ = (a);                                             \
        auto&& tf_binary_rhs_ = (b);                                             \
        if (!(tf_binary_lhs_ op tf_binary_rhs_)) {                               \
            ++::tf::current_failures();                                          \
            std::ostringstream tf_binary_oss_;                                   \
            tf_binary_oss_ << macro_name "(" #a ", " #b ")  实际: "          \
                           << ::tf::repr(tf_binary_lhs_) << failure_relation     \
                           << ::tf::repr(tf_binary_rhs_);                        \
            ::tf::report_failure(__FILE__, __LINE__, tf_binary_oss_.str());      \
            if (fatal) throw ::tf::AssertionFailure{};                           \
        }                                                                        \
    } while (0)

// ---- 非致命检查（失败后继续执行本用例） ------------------------------------
#define EXPECT_TRUE(cond)   TF_CHECK((cond),  false, "EXPECT_TRUE(" #cond ") 为假")
#define EXPECT_FALSE(cond)  TF_CHECK(!(cond), false, "EXPECT_FALSE(" #cond ") 为真")
#define EXPECT_EQ(a, b) TF_BINARY_CHECK(a, b, ==, false, "EXPECT_EQ", " != ")
#define EXPECT_NE(a, b) TF_BINARY_CHECK(a, b, !=, false, "EXPECT_NE", " == ")
#define EXPECT_LT(a, b) TF_BINARY_CHECK(a, b, <,  false, "EXPECT_LT", " >= ")
#define EXPECT_LE(a, b) TF_BINARY_CHECK(a, b, <=, false, "EXPECT_LE", " > ")
#define EXPECT_GT(a, b) TF_BINARY_CHECK(a, b, >,  false, "EXPECT_GT", " <= ")
#define EXPECT_GE(a, b) TF_BINARY_CHECK(a, b, >=, false, "EXPECT_GE", " < ")

// ---- 致命检查（失败立即中止本用例，避免后续解引用空指针等崩溃） ------------
#define ASSERT_TRUE(cond)   TF_CHECK((cond),  true,  "ASSERT_TRUE(" #cond ") 为假")
#define ASSERT_FALSE(cond)  TF_CHECK(!(cond), true,  "ASSERT_FALSE(" #cond ") 为真")
#define ASSERT_EQ(a, b) TF_BINARY_CHECK(a, b, ==, true, "ASSERT_EQ", " != ")
#define ASSERT_NE(a, b) TF_BINARY_CHECK(a, b, !=, true, "ASSERT_NE", " == ")

// ---- 异常检查 --------------------------------------------------------------
#define EXPECT_THROW(stmt, ExceptionType)                                       \
    do {                                                                        \
        ++::tf::total_checks();                                                  \
        bool tf_threw_ = false;                                                  \
        try { stmt; }                                                            \
        catch (const ExceptionType&) { tf_threw_ = true; }                       \
        catch (...) { tf_threw_ = false; }                                       \
        if (!tf_threw_) {                                                        \
            ++::tf::current_failures();                                          \
            ::tf::report_failure(__FILE__, __LINE__,                             \
                "EXPECT_THROW(" #stmt ", " #ExceptionType ") 未抛出预期异常");   \
        }                                                                        \
    } while (0)

#define EXPECT_NO_THROW(stmt)                                                   \
    do {                                                                        \
        ++::tf::total_checks();                                                  \
        try { stmt; }                                                            \
        catch (...) {                                                            \
            ++::tf::current_failures();                                          \
            ::tf::report_failure(__FILE__, __LINE__,                             \
                "EXPECT_NO_THROW(" #stmt ") 抛出了异常");                        \
        }                                                                        \
    } while (0)

// 主动让用例失败（带自定义信息）。例：FAIL_MSG("不应到达此分支");
#define FAIL_MSG(message) TF_CHECK(false, false, message)

// =============================================================================
//  自动生成的 main()
//    若某可执行文件需要自定义 main，可在 include 前 #define TF_NO_MAIN。
// =============================================================================
#ifndef TF_NO_MAIN
int main() {
    using namespace tf;
    enable_ansi_colors();

    int passed = 0;
    int failed = 0;

    std::printf("%s%s===== 运行 %zu 个测试用例 =====%s\n",
                color::bold(), color::green(), registry().size(), color::reset());

    for (const auto& tc : registry()) {
        current_failures() = 0;
        std::printf("%s[运行]%s %s.%s\n", color::dim(), color::reset(), tc.suite, tc.name);

        try {
            tc.fn();
        } catch (const AssertionFailure&) {
            // ASSERT_* 已记录失败，此处仅中止该用例。
        } catch (const std::exception& e) {
            ++current_failures();
            report_failure("", 0, std::string("用例抛出未捕获异常: ") + e.what());
        } catch (...) {
            ++current_failures();
            report_failure("", 0, "用例抛出未知类型异常");
        }

        if (current_failures() == 0) {
            ++passed;
            std::printf("  %s[通过]%s %s.%s\n", color::green(), color::reset(), tc.suite, tc.name);
        } else {
            ++failed;
            std::printf("  %s[失败]%s %s.%s (%d 处断言失败)\n",
                        color::red(), color::reset(), tc.suite, tc.name, current_failures());
        }
    }

    std::printf("%s---------------------------------%s\n", color::dim(), color::reset());
    std::printf("总计: %d 用例 | %s%d 通过%s | %s%d 失败%s | %d 次断言\n",
                passed + failed,
                color::green(), passed, color::reset(),
                (failed ? color::red() : color::dim()), failed, color::reset(),
                total_checks());

    if (failed == 0) {
        std::printf("%s%s全部通过 ✓%s\n", color::bold(), color::green(), color::reset());
    } else {
        std::printf("%s%s存在失败 ✗  —— 去实现 TODO 让它们变绿吧%s\n",
                    color::bold(), color::red(), color::reset());
    }
    return failed == 0 ? 0 : 1;
}
#endif // TF_NO_MAIN
