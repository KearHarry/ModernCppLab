// =============================================================================
//  A9 · 异常安全与 RAII：ScopeExit + 事务式容器
// -----------------------------------------------------------------------------
//  骨架会安全地“不执行清理、不提交事务”，所以测试预期为红，但不会泄漏
//  本模块自身管理的资源，也不会留下半修改状态。
// =============================================================================
#pragma once

#include <functional>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppbc {

// -----------------------------------------------------------------------------
// ScopeExit：离开当前作用域时执行一次清理动作；可 release 取消。
// 教学约定：清理函数必须 noexcept，析构函数绝不能把异常传播出去。
// -----------------------------------------------------------------------------
template <class F>
class ScopeExit {
public:
    // ===================== TODO(A9-1) 构造并武装 =====================
    // 将 action 移入 action_，并把 active_ 设为 true。
    // =================================================================
    explicit ScopeExit(F action) noexcept(std::is_nothrow_move_constructible_v<F>) {
        (void)action; // 安全占位：未武装
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

    // ===================== TODO(A9-2) 移动构造 =======================
    // 转移清理动作与 active 状态，再 release() 源，保证动作至多执行一次。
    // =================================================================
    ScopeExit(ScopeExit&& other) noexcept(std::is_nothrow_move_constructible_v<F>) {
        (void)other; // 安全占位：目标和源都未武装
    }

    ScopeExit& operator=(ScopeExit&&) = delete;

    // ===================== TODO(A9-3) 析构与 release =================
    // 析构时若 active_，调用 action_；release 只需取消 active_。
    // =================================================================
    ~ScopeExit() noexcept {
        // 安全占位
    }

    void release() noexcept {
        // 安全占位
    }

    bool active() const noexcept { return active_; }

private:
    std::optional<F> action_;
    bool active_ = false;
};

template <class F>
auto make_scope_exit(F&& action) {
    using Stored = std::decay_t<F>;
    return ScopeExit<Stored>(std::forward<F>(action));
}

// -----------------------------------------------------------------------------
// TransactionalVector：在副本上试做，全部成功后用 swap 原子式提交。
// 课程聚焦“强异常保证”，因此要求 T 可拷贝，且 vector::swap 对本分配器不抛。
// -----------------------------------------------------------------------------
template <class T>
class TransactionalVector {
public:
    TransactionalVector() = default;
    TransactionalVector(std::initializer_list<T> init) : values_(init) {}
    explicit TransactionalVector(std::vector<T> values) : values_(std::move(values)) {}

    const std::vector<T>& values() const noexcept { return values_; }
    std::size_t size() const noexcept { return values_.size(); }
    const T& operator[](std::size_t i) const { return values_[i]; }

    // ===================== TODO(A9-4) transact ========================
    // 1) 复制 values_ 得到 draft（失败则原对象不变）；
    // 2) std::invoke(operation, draft)（失败则销毁 draft，原对象不变）；
    // 3) values_.swap(draft) 提交。
    // =================================================================
    template <class Operation>
    void transact(Operation&& operation) {
        (void)operation; // 安全占位：不执行，也不改变原状态
    }

    // ===================== TODO(A9-5) append_all_strong ===============
    // 用 transact 在 draft 末尾插入 additions 的全部元素。
    // 不要直接逐个 push 到 values_，那只能提供基本保证。
    // =================================================================
    void append_all_strong(const std::vector<T>& additions) {
        (void)additions; // 安全占位
    }

private:
    std::vector<T> values_;
};

} // namespace cppbc
