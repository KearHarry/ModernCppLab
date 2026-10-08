// =============================================================================
//  A10 · Lambda、捕获与 std::invoke
// -----------------------------------------------------------------------------
//  骨架中的简单 lambda 返回确定的占位值；泛型/通用调用设施抛
//  UnimplementedInvoke。
//  因而所有 API 都能安全编译运行，但测试预期为红。
// =============================================================================
#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace cppbc {

struct UnimplementedInvoke : std::exception {
    const char* what() const noexcept override { return "invoke TODO is not implemented"; }
};

// ===================== TODO(A10-1) mutable 值捕获计数器 =============
// 返回一个按值捕获 current/step 的 mutable lambda：每次返回当前值，再加 step。
// 每个 lambda 副本应拥有彼此独立的捕获状态。
// 前置条件：每次更新后的值都能用 int 表示；超出范围前应停止调用。
// ===================================================================
inline auto make_counter(int start, int step = 1) {
    return [start, step]() mutable {
        (void)step;
        return start; // 安全占位：不推进
    };
}

// ===================== TODO(A10-2) 引用捕获 ==========================
// 返回一个按引用捕获 target、按值捕获 delta 的 lambda；调用时 target += delta，
// 并返回更新后的 target。调用方必须保证 target 比 lambda 活得久。
// 前置条件：target + delta 可用 int 表示。
// ===================================================================
inline auto make_reference_updater(int& target, int delta) {
    return [&target, delta]() {
        (void)delta;
        return target; // 安全占位：不修改
    };
}

// ===================== TODO(A10-3) 泛型 lambda =======================
// 返回 `[factor](auto value) { return value * factor; }`。
// 保留表达式自然推导出的结果类型，不要强制转成 int。
// ===================================================================
inline auto make_scaler(double factor) {
    return [factor](auto value) -> decltype(value * factor) {
        (void)value;
        (void)factor;
        // 只抛异常的占位不额外要求结果类型可默认构造，也保留引用返回类型。
        throw UnimplementedInvoke{};
    };
}

// ===================== TODO(A10-4) invoke_member =====================
// 用 std::invoke(std::forward<Member>(member), std::forward<Object>(object),
//                 std::forward<Args>(args)...) 统一调用：
//   - 成员函数指针；
//   - 数据成员指针；
//   - 对象、对象指针与 reference_wrapper。
// 显式返回 invoke_result_t 让“只抛异常”的骨架也能支持引用/void 返回类型。
// ===================================================================
template <class Member, class Object, class... Args>
auto invoke_member(Member&& member, Object&& object, Args&&... args)
    -> std::invoke_result_t<Member, Object, Args...> {
    (void)member;
    (void)object;
    (void)sizeof...(args);
    throw UnimplementedInvoke{}; // 安全占位
}

// InvokeBox：类型擦除前的“零额外分派”包装器。F 仍是模板参数，调用可内联；
// 它记录调用次数，并借 std::invoke 接受普通函数、lambda、仿函数和成员指针。
template <class F>
class InvokeBox {
public:
    explicit InvokeBox(F callable) : callable_(std::move(callable)) {}

    // ===================== TODO(A10-5) 完美转发并计数 ================
    // 先 ++calls_，再 `return std::invoke(callable_, std::forward<Args>(args)...);`
    // `return void表达式;` 在 R=void 时同样合法。
    // =================================================================
    template <class... Args>
    auto operator()(Args&&... args) -> std::invoke_result_t<F&, Args...> {
        (void)sizeof...(args);
        throw UnimplementedInvoke{}; // 安全占位
    }

    std::size_t call_count() const noexcept { return calls_; }

private:
    F callable_;
    std::size_t calls_ = 0;
};

template <class F>
auto make_invoke_box(F&& callable) {
    return InvokeBox<std::decay_t<F>>(std::forward<F>(callable));
}

} // namespace cppbc
