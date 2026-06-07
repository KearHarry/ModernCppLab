// =============================================================================
//  A3 · 模板元编程 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  在"编译期"用类型做计算。我们手写几个标准库 <type_traits> 里那样的工具，
//  外加可变参数模板、折叠表达式、C++20 concept。理解它们 = 理解标准库的魔法。
//
//  【几个关键套路（先有个印象，做题时再回头看）】
//   - 模板特化 = 编译期的 if：主模板是默认情况，特化针对特定类型给不同答案。
//   - trait    = 编译期函数：输入类型，输出 ::type（类型）或 ::value（常量）。
//   - SFINAE   = 替换失败不算错：探测"类型支不支持某操作"，支持就选这个分支。
//   - 折叠表达式 = 把 + / && 等运算一次性"折"到整包参数上。
//   - concept  = 给模板参数加"准入条件"，本质是个编译期 bool。
//
//  【为什么测试用 EXPECT 而不是 static_assert？】
//  static_assert 失败会直接编译不过，你就看不到"红"。所以这里把 ::value/::type
//  放到运行期 EXPECT 里检查——骨架阶段能编过、能跑、能看到红，再逐个写绿。
//
// =============================================================================
#pragma once

#include <cstddef>     // std::size_t
#include <type_traits> // std::true_type, std::false_type, std::void_t
#include <utility>     // std::declval

namespace cppbc {

// =============================================================================
//  1) RemoveReference<T> —— 剥掉引用，得到"裸类型"
//     这正是 std::move/std::forward 内部用到的基础工具。
//     例：RemoveReferenceT<int&> == int，RemoveReferenceT<int&&> == int
// =============================================================================
//  主模板：默认情况——T 本身就不是引用，原样返回。
template <class T>
struct RemoveReference {
    using type = T;
};

// ===================== TODO(A3-1) RemoveReference 的两个偏特化 =====================
// 在主模板【下方】补两个偏特化：
//   1) 当 T 是左值引用 U& 时，type 应为 U
//   2) 当 T 是右值引用 U&& 时，type 应为 U
//
//   写法（取消注释并补全）：
//     template <class U> struct RemoveReference<U&>  { using type = U; };
//     template <class U> struct RemoveReference<U&&> { using type = U; };
// ================================================================================

// 别名模板：让你能写 RemoveReferenceT<T> 而不是 typename RemoveReference<T>::type
template <class T>
using RemoveReferenceT = typename RemoveReference<T>::type;

// =============================================================================
//  2) IsSame<A, B> —— 两个类型是否完全相同
//     例：IsSameV<int, int> == true，IsSameV<int, double> == false
// =============================================================================
//  主模板：默认情况——两个不同类型，false。
template <class A, class B>
struct IsSame {
    static constexpr bool value = false;
};

// ===================== TODO(A3-2) IsSame 的"相等"偏特化 =====================
// 补一个偏特化：当两个类型参数是【同一个】 T 时，value 为 true。
//   template <class T> struct IsSame<T, T> { static constexpr bool value = true; };
// ==========================================================================

template <class A, class B>
inline constexpr bool IsSameV = IsSame<A, B>::value;

// =============================================================================
//  3) Conditional<B, T, F> —— 编译期三元运算符
//     B 为 true → type 是 T；B 为 false → type 是 F。
//     等价于标准库 std::conditional。
// =============================================================================
//  主模板：处理 B == true 的情况，type 选 T。
template <bool B, class T, class F>
struct Conditional {
    using type = T;
};

// ===================== TODO(A3-3) Conditional 的 false 偏特化 =====================
// 补一个偏特化：当 B 为 false 时，type 应选 F。
//   template <class T, class F> struct Conditional<false, T, F> { using type = F; };
// ===============================================================================

template <bool B, class T, class F>
using ConditionalT = typename Conditional<B, T, F>::type;

// =============================================================================
//  4) HasSize<T> —— 检测类型 T 有没有成员函数 .size()  （SFINAE / void_t 惯用法）
//     例：HasSizeV<std::string> == true，HasSizeV<int> == false
// =============================================================================
//  主模板：默认"没有"。第二个模板参数留作 SFINAE 的"探针位"，默认 void。
template <class T, class = void>
struct HasSize : std::false_type {};

// ===================== TODO(A3-4) HasSize 的 void_t 偏特化 =====================
// 补一个偏特化：只有当 "T 能调用 .size()" 时才命中，并继承 std::true_type。
// 原理：std::void_t<...> 不管里面是什么，只要里面的表达式合法就等于 void，
//   从而匹配上主模板第二参数的默认 void；若 .size() 不存在，decltype 替换失败
//   → 这个偏特化被丢弃（SFINAE）→ 落回主模板的 false。
//
//   template <class T>
//   struct HasSize<T, std::void_t<decltype(std::declval<T&>().size())>>
//       : std::true_type {};
//
//   提示：std::declval<T&>() 在 decltype 里"假装"造一个 T 的引用用于探测，
//        不会真的构造对象（只能用于不求值语境）。
// ============================================================================

template <class T>
inline constexpr bool HasSizeV = HasSize<T>::value;

// =============================================================================
//  5) sum(xs...) —— 可变参数求和（折叠表达式）
//     例：sum(1, 2, 3, 4) == 10
// =============================================================================
template <class... Ts>
auto sum(Ts... xs) {
    // ===================== TODO(A3-5) =====================
    // 用折叠表达式把所有参数加起来。建议用"二元左折叠"，这样 0 个参数也安全：
    //   return (0 + ... + xs);
    // 想一想：(xs + ...) 这种一元折叠，在 sum() 不传参时为什么会编译报错？
    // =====================================================
    return 0;  // ← 占位
}

// =============================================================================
//  6) all_of(flags...) —— 是否全部为真（逻辑折叠）
//     例：all_of(true, true, true) == true；all_of(true, false) == false
// =============================================================================
template <class... Ts>
bool all_of(Ts... flags) {
    // ===================== TODO(A3-6) =====================
    // 用折叠表达式把所有参数用 && 连起来：
    //   return (... && flags);   // 一元左折叠；空包时结果为 true（&& 的单位元）
    // =====================================================
    return true;  // ← 占位
}

// =============================================================================
//  7) count(args...) —— 参数个数（编译期已知）
//     例：count(1, 'a', 3.0) == 3
// =============================================================================
template <class... Ts>
constexpr std::size_t count(const Ts&...) {
    // ===================== TODO(A3-7) =====================
    // 返回参数包里参数的个数。提示：sizeof...(Ts)
    // =====================================================
    return 0;  // ← 占位
}

// =============================================================================
//  8) Addable<T> 概念 —— 类型 T 是否支持 a + a   （C++20 concept）
//     例：Addable<int> == true；Addable<某个没有 operator+ 的类型> == false
// =============================================================================
// ===================== TODO(A3-8) =====================
// 把下面这个"永远为真"的占位概念，改成真正的约束：T 必须能做 a + a。
//   写法：
//     template <class T>
//     concept Addable = requires(T a) { a + a; };
//   说明：requires(T a){ a + a; } 表示"存在一个 T 类型的 a，使表达式 a + a 合法"。
//        若不合法，concept 求值为 false（不会硬报错）。
// =====================================================
template <class T>
concept Addable = true;  // ← 占位：请替换成 requires(...) 约束

// twice：只接受满足 Addable 的类型（已给出，体会 concept 怎么约束模板参数）。
template <Addable T>
T twice(T x) {
    return x + x;
}

} // namespace cppbc
