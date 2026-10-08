// =============================================================================
//  A6 · 自己实现 Generator<T>（C++20 协程版"惰性序列"）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  用 C++20 协程做一个 Generator<T>：一个能被 range-for 遍历的**惰性序列**。函数体里
//  用 `co_yield x` 一个一个"吐"出值，调用方每取一个才**按需**算下一个——可以表达**无限**
//  序列（自然数、斐波那契）而不会一次性算完。这正是 Python 的 `yield`、C# 的
//  `IEnumerable` 在 C++ 里的对应物。
//
//  【协程三件套：promise / handle / awaitable】
//  一个协程函数（函数体里出现 co_yield/co_await/co_return）由编译器改写成状态机：
//    · 编译器为它分配一个**协程帧**，里面住着一个 promise_type 对象（你定义的）。
//    · promise_type 的几个固定成员是协程与外界的"接线盒"：
//        get_return_object()  协程刚启动时，造出返回给调用方的对象（这里是 Generator）。
//        initial_suspend()    启动后是否立刻挂起（这里 suspend_always：先停在入口，惰性）。
//        yield_value(v)       每次 `co_yield v` 时被调用——把 v 存起来，然后挂起。
//        final_suspend()      协程结束后是否挂起（suspend_always：让外部还能读状态再销毁）。
//        return_void()        `co_return;` / 落到函数末尾时调用。
//        unhandled_exception()协程体内抛了异常、没接住时调用。
//    · std::coroutine_handle<promise_type> 是协程帧的"遥控器"：resume() 续跑、done() 问
//      是否结束、destroy() 销毁、from_promise() 由 promise 反查到 handle。
//
//  【惰性怎么体现？】
//  initial_suspend 返回 suspend_always → 协程一启动就停在入口，一个值都还没算。之后每次
//  resume() 跑到下一个 co_yield 就又停下，把值交出来。所以"取一个、算一个"，无限序列也安全。
//
//  【迭代器适配：让 range-for 能用】
//  begin()：先 resume() 一次把协程推进到第一个值，返回一个包着 handle 的 iterator。
//  operator++：再 resume() 推进到下一个值；若协程结束了就变成"末尾"。
//  operator!=(end)：用 handle.done() 判断是否到末尾。end() 用 std::default_sentinel。
//  （迭代器与 begin/end 已给好，且对"空 handle"做了保护，骨架阶段不会崩。）
//
//  【你会实现的 3 个 TODO】（都在 promise_type 里——协程的"接线盒"）
//    A6-1  get_return_object —— 用 from_promise(*this) 拿到 handle，包进 Generator 返回
//    A6-2  yield_value(v)    —— 把 v 存进 current_，返回 suspend_always 挂起
//    A6-3  unhandled_exception —— 用 std::current_exception() 抓住异常，留给外部 rethrow
//
//  ⚠ 真实的 std::generator（C++23）还有递归 yield、自定义分配器、引用语义等；本模块聚焦
//     "协程帧 + promise + handle"的最小闭环。
//
// =============================================================================
#pragma once

#include <coroutine>
#include <exception>
#include <iterator>   // std::default_sentinel_t
#include <utility>    // std::move, std::exchange

namespace cppbc {

template <class T>
class Generator {
public:
    struct promise_type {
        T                  current_{};   // 最近一次 co_yield 出来的值
        std::exception_ptr exc_{};       // 协程体内抛出的异常（若有）

        // ===================== TODO(A6-1) get_return_object =============
        //  协程启动时调用：拿到本 promise 对应的 handle，包进 Generator 返回给调用方。
        //    return Generator{
        //        std::coroutine_handle<promise_type>::from_promise(*this) };
        // ===============================================================
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // 启动即挂起 / 结束后挂起：惰性 + 让外部读完状态再销毁。已给好。
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { return {}; }

        // ===================== TODO(A6-2) yield_value ==================
        //  每次 `co_yield v` 调用：存下值，然后挂起（把控制权交回调用方）。
        //    current_ = std::move(v);
        //    return {};                 // std::suspend_always
        // ===============================================================
        std::suspend_always yield_value(T v) {
            current_ = std::move(v);
            return {};
        }

        void return_void() noexcept {}

        // ===================== TODO(A6-3) unhandled_exception ==========
        //  协程体内抛异常且没接住时调用：抓住它，等迭代到末尾时由外部 rethrow。
        //    exc_ = std::current_exception();
        // ===============================================================
        void unhandled_exception() {
            exc_ = std::current_exception();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    // ---- 输入迭代器：包一个 handle，++ 即 resume 到下一个值 ----
    struct iterator {
        handle_type h_{};

        // 到末尾 = handle 空 或 协程已结束。对空 handle 安全。
        bool operator!=(std::default_sentinel_t) const {
            return h_ && !h_.done();
        }
        bool operator==(std::default_sentinel_t) const {
            return !(*this != std::default_sentinel);
        }
        iterator& operator++() {
            if (!h_) return *this;
            h_.resume();
            if (h_.done()) {
                auto e = h_.promise().exc_;
                if (e) std::rethrow_exception(e);   // 把协程体内的异常带到调用方
            }
            return *this;
        }
        const T& operator*() const { return h_.promise().current_; }
    };

    iterator begin() {
        if (h_) {
            h_.resume();                            // 推进到第一个 co_yield
            if (h_.done()) {
                auto e = h_.promise().exc_;
                if (e) std::rethrow_exception(e);
            }
        }
        return iterator{ h_ };
    }
    std::default_sentinel_t end() noexcept { return {}; }

    // ---- 生命周期：只可移动，析构销毁协程帧 ----
    Generator() noexcept : h_(nullptr) {}
    explicit Generator(handle_type h) noexcept : h_(h) {}
    Generator(Generator&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    Generator& operator=(Generator&& o) noexcept {
        if (this != &o) {
            if (h_) h_.destroy();
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
    Generator(const Generator&)            = delete;
    Generator& operator=(const Generator&) = delete;
    ~Generator() { if (h_) h_.destroy(); }

    bool valid() const noexcept { return static_cast<bool>(h_); }

private:
    handle_type h_;
};

} // namespace cppbc
