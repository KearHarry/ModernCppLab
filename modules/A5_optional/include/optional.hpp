// =============================================================================
//  A5 · 自己实现 Optional<T>（std::optional 的微缩版）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现 std::optional<T>：一个"要么装着一个 T、要么什么都没有"的盒子。它在**不借助堆、
//  不用裸指针**的前提下表达"可能没有值"。典型用途：查找可能失败（返回 Optional 而非
//  魔法值 -1）、配置项可缺省、延迟初始化的成员……比"用 -1/nullptr/bool 标志位"安全得多。
//
//  【核心难点：在"原地存储"里手动管理对象生命周期】
//  Optional 必须能存下一个 T，但当它"空"时又不能真的有一个 T 活着。做法：用一个**联合体**
//  （union）当存储——union 成员**不会被自动构造/析构**，给了我们完全的手动控制权：
//      union { T value_; };   bool engaged_;     // engaged_=true 时 value_ 才是"活的"
//  · 放值：在 value_ 的内存上 **placement new** 一个 T，engaged_=true。
//  · 取空：调用 value_ 的**析构函数** value_.~T()，engaged_=false。
//  这套"原地构造 + 显式析构"正是 C2 的 SSO、A4 的小缓冲、std::variant 共用的底层手法。
//
//  【为什么不能简单地 `T value_; bool engaged_;`？】
//  那样 value_ 永远是个被构造好的对象——"空"状态下也占着一个真实 T（还得能默认构造），
//  析构时机也不受控。用 union 才能做到"空时内存里没有活着的 T"，这正是 optional 的语义。
//
//  【特殊成员函数要"按状态"手动搬运】
//  因为 union 不帮你管 value_，拷贝/移动/赋值/析构都得你亲自做，且**只在 engaged_ 时**
//  才碰 value_：
//    拷贝构造：对方有值 → 在自己存储上拷贝构造一个；对方空 → 自己也空。
//    析构：有值 → 析构 value_。（本模块用 reset() 统一处理）
//  漏了任何一处，就会内存泄漏（没析构）或重复析构/读垃圾（错误地碰了空值）。
//
//  【你会实现的 3 个 TODO】（默认/值构造、移动、赋值、value/operator*、value_or 等已给好）
//    A5-1  reset()        —— 有值则显式析构 value_、置空（生命周期的"收"）
//    A5-2  emplace(args…)  —— 先 reset，再 placement new 就地构造，置 engaged（生命周期的"放"）
//    A5-3  Optional(const Optional&) 拷贝构造 —— 对方有值则就地拷贝构造，否则保持空
//
//  ⚠ 进阶细节：严格别名/`std::launder`、条件 noexcept、trivially-destructible 时可平凡析构
//     等，真实 std::optional 还会处理；本模块聚焦最核心的"原地生命周期管理"，其余在 README。
//
// =============================================================================
#pragma once

#include <exception>  // std::exception
#include <new>        // placement new
#include <utility>    // std::move, std::forward

namespace cppbc {

// 访问空 Optional 的 value() 时抛出（对应 std::bad_optional_access）。
struct BadOptionalAccess : std::exception {
    const char* what() const noexcept override { return "bad optional access"; }
};

// ---------------------------------------------------------------------------
//  Optional<T>：原地存储一个可有可无的 T。要求 T 可被相应地构造/拷贝/移动。
// ---------------------------------------------------------------------------
template <class T>
class Optional {
    union {
        T value_;          // 仅当 engaged_ 为真时，这里才有一个"活着"的 T
    };
    bool engaged_ = false; // 是否装着值

public:
    // 空构造：不构造 value_（union 成员不自动构造），engaged_ 默认 false。
    Optional() noexcept {}

    // 值构造：用一个 T 初始化（委托给 emplace，复用同一套构造逻辑）。已给好。
    Optional(const T& v) { emplace(v); }
    Optional(T&& v)      { emplace(std::move(v)); }

    // 析构：有值则析构它（统一走 reset）。已给好。
    ~Optional() { reset(); }

    // 移动构造：对方有值则就地移动构造一个（保持对方仍 engaged，与 std::optional 一致）。已给好。
    Optional(Optional&& other) noexcept(noexcept(T(std::move(other.value_)))) {
        if (other.engaged_) {
            ::new (static_cast<void*>(&value_)) T(std::move(other.value_));
            engaged_ = true;
        }
    }

    // 拷贝赋值 / 移动赋值：先清空自己，再按对方状态搬运。已给好（复用 reset + emplace）。
    Optional& operator=(const Optional& other) {
        if (this != &other) {
            reset();
            if (other.engaged_) emplace(other.value_);
        }
        return *this;
    }
    Optional& operator=(Optional&& other)
        noexcept(noexcept(T(std::move(other.value_)))) {
        if (this != &other) {
            reset();
            if (other.engaged_) emplace(std::move(other.value_));
        }
        return *this;
    }

    // ===================== TODO(A5-1) reset =========================
    //  若当前有值：显式调用析构函数销毁它，并置为空。空盒子调用是安全的空操作。
    //    if (engaged_) {
    //        value_.~T();        // 显式析构（union 成员不会自动析构）
    //        engaged_ = false;
    //    }
    // ===============================================================
    void reset() noexcept {
        // TODO
    }

    // ===================== TODO(A5-2) emplace =======================
    //  就地构造一个新值（先清掉旧值），返回对它的引用。这是"放值"的核心：placement new。
    //    reset();                                                   // 先销毁可能存在的旧值
    //    ::new (static_cast<void*>(&value_)) T(std::forward<Args>(args)...);  // 在原地构造
    //    engaged_ = true;
    //    return value_;
    // ===============================================================
    template <class... Args>
    T& emplace(Args&&... args) {
        // TODO
        ((void)args, ...);
        return value_;   // 骨架：未真正构造（engaged_ 仍为 false，靠 has_value 挡住访问）
    }

    // ===================== TODO(A5-3) 拷贝构造 ======================
    //  对方有值 → 在自己的存储上拷贝构造一个；对方空 → 自己保持空。
    //    if (other.engaged_) {
    //        ::new (static_cast<void*>(&value_)) T(other.value_);   // 拷贝构造
    //        engaged_ = true;
    //    }
    //  （也可直接写成 `if (other.engaged_) emplace(other.value_);`，复用 A5-2。）
    // ===============================================================
    Optional(const Optional& other) {
        // TODO
        (void)other;
    }

    // ---- 以下均已给好 ----
    bool has_value() const noexcept { return engaged_; }
    explicit operator bool() const noexcept { return engaged_; }

    // 无检查访问（前置条件：有值）。空盒子上调用是未定义行为——和 std::optional 一致。
    T&       operator*()        noexcept { return value_; }
    const T& operator*()  const noexcept { return value_; }
    T*       operator->()       noexcept { return &value_; }
    const T* operator->() const noexcept { return &value_; }

    // 受检访问：空则抛 BadOptionalAccess。
    T& value() {
        if (!engaged_) throw BadOptionalAccess{};
        return value_;
    }
    const T& value() const {
        if (!engaged_) throw BadOptionalAccess{};
        return value_;
    }

    // 有值则返回值的拷贝，否则返回给定的默认值。
    T value_or(T default_value) const {
        return engaged_ ? value_ : default_value;
    }
};

} // namespace cppbc
