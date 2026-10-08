// =============================================================================
//  A7 · 类型推导：auto / decltype / 模板参数推导 / 退化
// -----------------------------------------------------------------------------
//  各探针把编译期类型映射成可观察的 TypeTag；TODO 注释保留目标与推导理由，
//  TypeTag::Unknown 则作为未覆盖类型或未完成分支的安全回退。
// =============================================================================
#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace cppbc {

// 用运行期枚举承载“编译期推导结果”。这样某个推导分支写错时测试只会红，
// 不会像 static_assert 那样直接阻止整个课程模块编译。
enum class TypeTag {
    Unknown,
    Int,
    ConstInt,
    IntLRef,
    ConstIntLRef,
    IntRRef,
    IntPointer,
    ConstIntPointer,
    IntArray3,
    IntFunction,
    IntFunctionPointer
};

// ===================== TODO(A7-1) type_tag ==========================
// 根据 T 返回对应标签。推荐使用 if constexpr + type_traits：
//   std::is_same_v<T, int>、std::is_same_v<T, const int> ……
// 注意：这里故意要求区分引用、数组、函数与函数指针。
// ===================================================================
template <class T>
constexpr TypeTag type_tag() noexcept {
    if constexpr (std::is_same_v<T, int>) return TypeTag::Int;
    else if constexpr (std::is_same_v<T, const int>) return TypeTag::ConstInt;
    else if constexpr (std::is_same_v<T, int&>) return TypeTag::IntLRef;
    else if constexpr (std::is_same_v<T, const int&>) return TypeTag::ConstIntLRef;
    else if constexpr (std::is_same_v<T, int&&>) return TypeTag::IntRRef;
    else if constexpr (std::is_same_v<T, int*>) return TypeTag::IntPointer;
    else if constexpr (std::is_same_v<T, const int*>) return TypeTag::ConstIntPointer;
    else if constexpr (std::is_same_v<T, int[3]> || std::is_same_v<T, int (&)[3]>)
        return TypeTag::IntArray3;
    else if constexpr (std::is_same_v<T, int(int)> || std::is_same_v<T, int (&)(int)>)
        return TypeTag::IntFunction;
    else if constexpr (std::is_same_v<T, int (*)(int)>) return TypeTag::IntFunctionPointer;
    else return TypeTag::Unknown;
}

// ===================== TODO(A7-2) 按值模板推导 ======================
// 对 template<class T> f(T value)，顶层 const 会丢失，数组/函数会退化成指针。
// 返回 type_tag<T>() 即可。
// ===================================================================
template <class T>
constexpr TypeTag deduce_by_value(T) noexcept {
    return type_tag<T>();
}

// ===================== TODO(A7-3) 左值引用模板推导 ==================
// T& 不发生数组/函数退化，并保留被引用对象的 const。
// 返回 type_tag<T&>()。
// ===================================================================
template <class T>
constexpr TypeTag deduce_by_lref(T&) noexcept {
    return type_tag<T&>();
}

// ===================== TODO(A7-4) 转发引用模板推导 ==================
// T&& 且 T 被推导时是转发引用：传左值时 T 推成 U&，传右值时推成 U。
// 用引用折叠后的 T&& 做标签：type_tag<T&&>()。
// ===================================================================
template <class T>
constexpr TypeTag deduce_by_forward(T&&) noexcept {
    return type_tag<T&&>();
}

// ===================== TODO(A7-5) decltype(auto) 保留引用 ===========
// 如果误用 auto 会按值返回，修改返回值不会影响原容器。
// 返回类型应为 decltype(auto)，并写 return (c[index]);。
// 圆括号很重要：decltype((表达式)) 按值类别得到 T&。
// ===================================================================
template <class Container>
decltype(auto) element(Container& c, std::size_t index) {
    return (c[index]);
}

// ===================== TODO(A7-6) decltype 规则探针 ==================
// decltype(name) 对“无括号名字”给出声明类型；decltype((name)) 按表达式
// 值类别给出引用。请让两个函数分别返回正确的标签。
// ===================================================================
template <class T>
constexpr TypeTag decltype_name(T&& value) noexcept {
    (void)value;
    return type_tag<decltype(value)>();
}

template <class T>
constexpr TypeTag decltype_expr(T&& value) noexcept {
    (void)value;
    return type_tag<decltype((value))>();
}

} // namespace cppbc
