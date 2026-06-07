// =============================================================================
//  A4 · 类型擦除与手写 std::function（type erasure）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  亲手实现一个简化版的 `std::function`，叫 `Function<R(Args...)>`。它能装下
//  **任何**「调用起来像 R(Args...) 的东西」——普通函数指针、lambda、带状态的仿函数
//  (functor)、bind 结果……然后像普通函数一样 `f(参数)` 调用。这背后的核心技术
//  叫 **类型擦除(type erasure)**，是大厂面试里区分"会用 STL"和"懂 STL 怎么造出来"
//  的经典题，也是理解 std::function / std::any / std::shared_ptr 删除器的钥匙。
//
//  【什么是"类型擦除"？为什么需要它？】
//  矛盾点：`Function<int(int,int)>` 是**一个固定类型**，但它要能装下千奇百怪、
//  类型各不相同的可调用对象——一个 lambda 和另一个 lambda 即使长得一样，类型也不同；
//  函数指针又是另一种类型。我们不可能把"具体类型 F"写进 `Function` 的类型里
//  （否则就不通用了）。于是要把 F **擦掉**：对外只暴露统一的"能被调用"这一抽象能力。
//
//  【怎么擦？—— 虚函数 + 模板派生类（经典三件套）】
//    1) 定义一个**抽象接口** `CallableBase`：只规定"能 invoke、能 clone、能析构"，
//       完全不提具体类型 F。
//    2) 定义一个**模板派生类** `CallableImpl<F>`：它把具体的 F 存进成员 `f_`，并
//       重写 invoke()=调用 f_、clone()=复制自己。**F 只活在这个派生类内部。**
//    3) `Function` 内部只持有一根 **基类指针** `unique_ptr<CallableBase>`。
//       构造时 new 一个 `CallableImpl<F>`，把 F "藏"进派生类、对外只剩基类指针——
//       F 就被"擦除"了。调用时经基类指针虚调用 invoke()，多态分派回真正的 f_。
//
//      Function<int(int,int)> f = some_lambda;
//
//      ┌─ Function ─────────┐        ┌─ CallableImpl<Lambda> ─┐
//      │ callable_ ───────────────▶  │ (基类 CallableBase)    │
//      └────────────────────┘        │ f_ = some_lambda       │  ← 具体类型藏在这里
//                                     │ invoke(): f_(a,b)      │
//                                     │ clone():  new 自己     │
//                                     └────────────────────────┘
//
//  这其实就是 D4 学的 **vtable 动态分派** 换了个场景用：用"基类指针 + 虚函数"把
//  "编译期千变万化的类型"收敛成"运行期统一的接口"。代价是一次堆分配 + 一次间接调用，
//  换来"一个类型装下万物"的灵活性。（真实 std::function 还会做小对象优化 SBO，把小
//  的可调用对象直接塞进自身缓冲区、省掉堆分配——本模块先不做，理解类型擦除是重点。）
//
//  【为什么需要 clone？】
//  std::function 是**可拷贝**的。但 `Function` 只有一根基类指针，直接拷指针会变成两
//  个对象共享同一份 callable（double free）。又因为我们擦除了 F，普通拷贝构造"不知道
//  要复制成什么具体类型"。解法和 D4 的虚拷贝一样：让基类提供虚函数 `clone()`，每个
//  `CallableImpl<F>` 重写它 `new 一个同类型的自己`，于是 `callable_->clone()` 能多态地
//  复制出正确类型——又是**原型模式**。
//
//  【你会实现的 4 个 TODO】（签名都给好了，你只填函数体）
//    A4-1  CallableImpl<F>::invoke   —— 真正调用被擦除的 f_
//    A4-2  CallableImpl<F>::clone    —— 复制出一个同类型的自己（虚拷贝）
//    A4-3  Function 的转换构造函数    —— 把任意可调用对象 F 包进 CallableImpl<F>（擦除发生处）
//    A4-4  Function::operator()       —— 经基类指针虚调用 invoke，对外像普通函数
//
// =============================================================================
#pragma once

#include <memory>       // std::unique_ptr / make_unique
#include <type_traits>  // std::decay_t / std::is_same_v / enable_if
#include <utility>      // std::move / std::forward

namespace cppbc {

// 主模板只声明不定义：强制使用者写成 Function<R(Args...)> 这种"函数类型"特化形式。
template <class Signature>
class Function;  // 未定义

// ---------------------------------------------------------------------------
//  针对"函数类型 R(Args...)"的偏特化：这才是真正的实现。
// ---------------------------------------------------------------------------
template <class R, class... Args>
class Function<R(Args...)> {
    // ---- 抽象接口：对外只暴露"能调用 / 能克隆 / 能析构"，不含任何具体类型 ----
    struct CallableBase {
        virtual R invoke(Args... args) const = 0;                 // 调用被擦除的目标
        virtual std::unique_ptr<CallableBase> clone() const = 0;  // 虚拷贝（原型模式）
        virtual ~CallableBase() = default;                        // 基类析构必须 virtual
    };

    // ---- 模板派生类：把具体可调用类型 F 藏在这里（类型擦除的"藏匿点"）----
    template <class F>
    struct CallableImpl final : CallableBase {
        F f_;  // 被擦除的真身：lambda / 函数指针 / 仿函数……
        explicit CallableImpl(F f) : f_(std::move(f)) {}

        // ===================== TODO(A4-1) invoke ====================
        //  调用存起来的 f_，把参数透传进去并返回结果：
        //      return f_(args...);
        //  （args 已经是本函数的形参，直接转发即可；R 为 void 时 return 表达式也合法）
        // ============================================================
        R invoke(Args... args) const override {
            // TODO
            (void)sizeof...(args);   // 占位：避免未使用形参告警
            return R{};              // 骨架返回默认值；实现后改为 return f_(args...);
        }

        // ===================== TODO(A4-2) clone =====================
        //  复制出一个"同样是 CallableImpl<F>"的新对象（虚拷贝 / 原型模式）：
        //      return std::make_unique<CallableImpl>(f_);
        //  注意返回类型是基类指针 unique_ptr<CallableBase>，派生→基类隐式转换成立。
        // ============================================================
        std::unique_ptr<CallableBase> clone() const override {
            // TODO
            return nullptr;
        }
    };

    std::unique_ptr<CallableBase> callable_;  // 唯一的数据成员：一根基类指针

public:
    // 空 Function（未绑定任何目标）。调用空 Function 是未定义行为，调用前可用 bool 判空。
    Function() noexcept = default;
    Function(std::nullptr_t) noexcept {}

    // ===================== TODO(A4-3) 转换构造函数 ==================
    //  把"任意可调用对象 f"包进 CallableImpl<F>，再交给基类指针 callable_ 持有——
    //  **类型擦除就发生在这一行**：F 进去时还是具体类型，出来时只剩基类指针。
    //      callable_ = std::make_unique<CallableImpl<F>>(std::move(f));
    //
    //  签名里的 enable_if 是一道"防自我吞噬"的护栏：当 f 本身就是另一个 Function 时，
    //  不要走这个模板构造（否则会和拷贝/移动构造打架），把它让给下面的拷贝/移动构造。
    //  （这是 std::function 真实实现里也存在的经典坑，叫"完美转发构造劫持"。）
    // ===============================================================
    template <class F,
              class = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Function>>>
    Function(F f) {
        // TODO
        (void)f;  // 占位：实现后删掉，改为把 f 包进 CallableImpl<F> 存入 callable_
    }

    // ---- 拷贝：用虚函数 clone() 多态地深拷贝；空对象拷成空对象 ----
    Function(const Function& o)
        : callable_(o.callable_ ? o.callable_->clone() : nullptr) {}
    Function& operator=(const Function& o) {
        callable_ = o.callable_ ? o.callable_->clone() : nullptr;
        return *this;
    }

    // ---- 移动：直接转移基类指针所有权，廉价 ----
    Function(Function&&) noexcept = default;
    Function& operator=(Function&&) noexcept = default;

    // ===================== TODO(A4-4) operator() ===================
    //  对外像普通函数一样调用：经基类指针虚调用 invoke()，多态分派回真正的 f_。
    //      return callable_->invoke(args...);
    //  （前提是已绑定目标；空 Function 调用属未定义行为，与 std::function 一致）
    // ===============================================================
    R operator()(Args... args) const {
        // TODO
        (void)sizeof...(args);
        return R{};  // 骨架返回默认值；实现后改为 return callable_->invoke(args...);
    }

    // 是否已绑定目标（仿 std::function 的显式 bool 转换）。
    explicit operator bool() const noexcept { return callable_ != nullptr; }
};

} // namespace cppbc
