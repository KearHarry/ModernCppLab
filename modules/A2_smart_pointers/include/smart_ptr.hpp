// =============================================================================
//  A2 · 智能指针 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  手写三个智能指针：UniquePtr / SharedPtr / WeakPtr。它们都是 RAII 工具：
//  对象构造时"拿住"一块堆内存，析构时自动 delete，帮你免去手动管理内存。
//
//  【三者的区别（一句话版）】
//   - UniquePtr：独占。一块内存只能有一个主人，不能拷贝，只能"交钥匙"(移动)。
//   - SharedPtr：共享。可以很多人一起用，靠"引用计数"记着还剩几个人，归 0 才删。
//   - WeakPtr  ：旁观。只看不持有，不影响计数；用来打破 SharedPtr 的循环引用。
//
//  【预备词汇】
//   - 控制块 (control block)：SharedPtr 背后的一小块管理数据，存引用计数。
//   - strong 计数：有几个 SharedPtr 在持有对象（归 0 → 删对象）。
//   - weak   计数：有几个 WeakPtr 在观察（+ 所有 shared 合起来算 1）。归 0 → 删控制块。
//   - 原子 (std::atomic)：多线程下也能安全 ++/-- 的整数。
//   - CAS：compare_exchange，原子的"比较并交换"，无锁编程的基石。
//
//  【做题顺序建议】先做 UniquePtr(A2-1~5) → 再 SharedPtr(A2-6~12) → 最后 WeakPtr(A2-13~15)。
//
// =============================================================================
#pragma once

#include <atomic>    // std::atomic：线程安全的计数器
#include <cstddef>   // std::size_t
#include <utility>   // std::move, std::forward

namespace cppbc {

// =============================================================================
//  Part 1 · UniquePtr<T> —— 独占所有权
// =============================================================================
//
//  核心思想：一块内存只能有一个 UniquePtr 拥有。
//   - 禁止拷贝（拷贝就有两个主人 → 析构时 delete 两次 → 崩溃）
//   - 允许移动（把所有权从一个对象"转交"给另一个，源对象交出指针后置空）
//
template <class T>
class UniquePtr {
public:
    // 默认构造：不持有任何对象，ptr_ = nullptr。
    UniquePtr() noexcept = default;

    // 从裸指针接管所有权。explicit 禁止 UniquePtr<T> p = rawPtr; 这种隐式写法。
    explicit UniquePtr(T* p) noexcept : ptr_(p) {}

    // ---- 禁止拷贝（这正是 unique 的含义，已用 = delete 删掉） ----
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // ===================== TODO(A2-1) 移动构造 =======================
    // 把 other 的所有权"偷"过来：拿走它的指针，再把它置空。
    //   步骤：ptr_ = other.ptr_;  other.ptr_ = nullptr;
    //   为什么要置空 other？否则 other 析构时会 delete 掉我们刚接管的指针。
    // ===============================================================
    UniquePtr(UniquePtr&& other) noexcept {
        // TODO: 实现移动构造
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }

    // ===================== TODO(A2-2) 移动赋值 =======================
    // 已存在的对象接管 other 的所有权。
    //   1) 自赋值检查：if (this == &other) return *this;
    //   2) 先 delete 自己当前持有的 ptr_（否则泄漏）；
    //   3) 偷走 other 的指针并把 other 置空；
    //   4) return *this;
    // ===============================================================
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        // TODO: 实现移动赋值
        if (this == &other) {
            return *this;
        }
        delete ptr_;
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
        return *this;
    }

    // ===================== TODO(A2-3) 析构 ===========================
    // 释放持有的内存。delete nullptr 是安全的，无需判空。
    // ===============================================================
    ~UniquePtr() {
        // TODO: delete ptr_;
        delete ptr_;
    }

    // ---- 访问器（已给出） ----
    T* get() const noexcept { return ptr_; }
    T& operator*()  const { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // ===================== TODO(A2-4) release ========================
    // "放弃所有权"但【不删除】对象：把内部指针置空并返回原指针，
    // 之后由调用者负责 delete。
    //   T* old = ptr_;  ptr_ = nullptr;  return old;
    // ===============================================================
    T* release() noexcept {
        // TODO
        T* old = ptr_;
        ptr_ = nullptr;
        return old;
    }

    // ===================== TODO(A2-5) reset ==========================
    // 改为管理新指针 p（默认 nullptr）：先释放旧的，再接管新的。
    //   T* old = ptr_;  ptr_ = p;  delete old;
    //   （先换指针再删旧的，可避免一些自赋值/异常的微妙问题）
    // ===============================================================
    void reset(T* p = nullptr) noexcept {
        // TODO
        T* old = ptr_;
        ptr_ = p;
        delete old;
    }

private:
    T* ptr_ = nullptr;
};

// MakeUnique：完美转发构造参数，等价于 std::make_unique（已给出，体会下转发）。
template <class T, class... Args>
UniquePtr<T> MakeUnique(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

// =============================================================================
//  Part 2 · SharedPtr<T> —— 共享所有权（引用计数）
// =============================================================================

namespace detail {
// 控制块：被同一对象的所有 SharedPtr / WeakPtr 共享。已给出，无需修改。
//   strong：还有几个 SharedPtr 在持有对象。
//   weak  ：还有几个 WeakPtr 在观察 +（strong>0 时所有 shared 合计贡献的 1）。
//   ptr   ：被管理的对象指针（strong 归 0 时由我们 delete）。
template <class T>
struct ControlBlock {
    std::atomic<long> strong;
    std::atomic<long> weak;
    T* ptr;
    explicit ControlBlock(T* p) noexcept : strong(1), weak(1), ptr(p) {}
};
} // namespace detail

template <class T> class WeakPtr;  // 前置声明，SharedPtr 里要 friend 它

template <class T>
class SharedPtr {
public:
    // 默认构造：空。
    SharedPtr() noexcept = default;

    // 从裸指针构造：new 一个控制块，strong=weak=1。已给出。
    explicit SharedPtr(T* p)
        : ptr_(p), cb_(p ? new detail::ControlBlock<T>(p) : nullptr) {}

    // ===================== TODO(A2-6) 拷贝构造 =======================
    // 共享 other 指向的对象：复制 ptr_ 与 cb_，然后让 strong 计数 +1。
    //   ptr_ = other.ptr_;  cb_ = other.cb_;  retain_();
    //   （retain_() 已给出，负责安全地把 strong + 1）
    // ===============================================================
    SharedPtr(const SharedPtr& other) noexcept {
        ptr_ = other.ptr_;
        cb_ = other.cb_;
        retain_();
    }

    // ===================== TODO(A2-7) 拷贝赋值 =======================
    // 让自己改为共享 other 的对象。
    //   1) 自赋值检查：if (this == &other) return *this;
    //   2) 先 release_() 放掉自己当前持有的（strong-1，必要时销毁）；
    //   3) 复制 other 的 ptr_/cb_，再 retain_()（strong+1）；
    //   4) return *this;
    // ===============================================================
    SharedPtr& operator=(const SharedPtr& other) noexcept {
        // TODO
        if(this == &other) return *this;
        release_();
        ptr_ = other.ptr_;
        cb_ = other.cb_;
        retain_();
        return *this;
    }

    // ===================== TODO(A2-8) 移动构造 =======================
    // 偷走 other 的 ptr_/cb_，并把 other 置空（计数总量不变，不需要 ++）。
    // ===============================================================
    SharedPtr(SharedPtr&& other) noexcept {
        ptr_ = other.ptr_;
        cb_ = other.cb_;
        other.ptr_ = nullptr;
        other.cb_ = nullptr;
        // TODO
    }

    // ===================== TODO(A2-9) 移动赋值 =======================
    //   1) 自赋值检查；
    //   2) release_() 放掉自己当前持有的；
    //   3) 偷走 other 的 ptr_/cb_ 并把 other 置空；
    //   4) return *this;
    // ===============================================================
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if(this == &other) return *this;
        release_();
        ptr_ = other.ptr_;
        cb_ = other.cb_;
        other.ptr_ = nullptr;
        other.cb_ = nullptr;
        // TODO
        return *this;
    }

    // ===================== TODO(A2-10) 析构 ==========================
    // 调用 release_() 即可（把强引用减一，必要时触发销毁）。
    // ===============================================================
    ~SharedPtr() {
        release_();
    }

    // ===================== TODO(A2-11) reset =========================
    // 放弃当前持有，变为空：release_() 之后把 ptr_/cb_ 置空。
    // ===============================================================
    void reset() noexcept {
        release_();
        ptr_ = nullptr;
        cb_ = nullptr;
    }

    // ---- 访问器（已给出） ----
    long use_count() const noexcept { return cb_ ? cb_->strong.load() : 0; }
    T*   get()       const noexcept { return ptr_; }
    T&   operator*()  const { return *ptr_; }
    T*   operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    // retain_：强引用 +1（已给出）。relaxed 足够：仅是计数，不借它同步其它数据。
    void retain_() noexcept {
        if (cb_) cb_->strong.fetch_add(1, std::memory_order_relaxed);
    }

    // ===================== TODO(A2-12) release_ ======================
    // 【本模块的灵魂】两阶段销毁：
    //   if (!cb_) return;
    //   // 第一阶段：强引用减一。fetch_sub 返回"减之前"的旧值。
    //   if (cb_->strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    //       // 我是最后一个 SharedPtr → 销毁被管理对象
    //       delete cb_->ptr;
    //       cb_->ptr = nullptr;
    //       // 第二阶段：释放"所有 shared 合计贡献的那 1 个 weak"
    //       if (cb_->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    //           delete cb_;   // 连 weak 也没了 → 回收控制块
    //       }
    //   }
    //   想一想：为什么用 acq_rel？因为"销毁对象"这件事必须在所有线程的
    //          "减计数"都完成后才发生，需要 release 保证之前的写可见、
    //          acquire 保证看到最后一次减。
    // ===============================================================
    void release_() noexcept {
        if (!cb_) return;
        if (cb_->strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete cb_->ptr;
            cb_->ptr = nullptr;
            if (cb_->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete cb_;
            }
        }
        // TODO
    }

    // 供 WeakPtr::lock() 使用的私有构造：直接接管 (ptr, cb) 且【不再 ++strong】
    // （因为 lock 里已用 CAS 把 strong +1 了）。
    SharedPtr(T* p, detail::ControlBlock<T>* cb) noexcept : ptr_(p), cb_(cb) {}

    template <class U> friend class WeakPtr;  // WeakPtr 需要读 ptr_/cb_、用私有构造

    T* ptr_ = nullptr;
    detail::ControlBlock<T>* cb_ = nullptr;
};

// MakeShared：等价于 std::make_shared 的简化版（教学用，仍是两次分配）。已给出。
template <class T, class... Args>
SharedPtr<T> MakeShared(Args&&... args) {
    return SharedPtr<T>(new T(std::forward<Args>(args)...));
}

// =============================================================================
//  Part 3 · WeakPtr<T> —— 弱引用（只观察，不持有）
// =============================================================================
//
//  WeakPtr 不增加 strong 计数，所以不会阻止对象被销毁。它增加 weak 计数，
//  保证"控制块"在自己存活期间不被回收（这样它才能安全地查看对象是否还在）。
//
template <class T>
class WeakPtr {
public:
    WeakPtr() noexcept = default;

    // ===================== TODO(A2-13) 从 SharedPtr 构造 =============
    // 观察 sp 指向的对象：复制 sp 的 ptr_/cb_，并让 weak 计数 +1。
    //   ptr_ = sp.ptr_;  cb_ = sp.cb_;
    //   if (cb_) cb_->weak.fetch_add(1, std::memory_order_relaxed);
    //   （SharedPtr 已 friend 本类，可直接访问 sp.ptr_ / sp.cb_）
    // ===============================================================
    WeakPtr(const SharedPtr<T>& sp) noexcept {
        ptr_ = sp.ptr_;
        cb_ = sp.cb_;
        if (cb_) cb_->weak.fetch_add(1, std::memory_order_relaxed);
        // TODO
    }

    // ---- 拷贝/移动/赋值（已给出，它们都正确地维护 weak 计数） ----
    WeakPtr(const WeakPtr& other) noexcept : ptr_(other.ptr_), cb_(other.cb_) {
        if (cb_) cb_->weak.fetch_add(1, std::memory_order_relaxed);
    }
    WeakPtr(WeakPtr&& other) noexcept : ptr_(other.ptr_), cb_(other.cb_) {
        other.ptr_ = nullptr;
        other.cb_  = nullptr;
    }
    WeakPtr& operator=(const WeakPtr& other) noexcept {
        if (this != &other) {
            release_weak_();
            ptr_ = other.ptr_;
            cb_  = other.cb_;
            if (cb_) cb_->weak.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }
    WeakPtr& operator=(WeakPtr&& other) noexcept {
        if (this != &other) {
            release_weak_();
            ptr_ = other.ptr_;
            cb_  = other.cb_;
            other.ptr_ = nullptr;
            other.cb_  = nullptr;
        }
        return *this;
    }

    // 析构：弱引用减一（已给出，调用你要实现的 release_weak_）。
    ~WeakPtr() { release_weak_(); }

    // 对象是否已被销毁（strong 是否已归 0）。已给出。
    bool expired() const noexcept {
        return !cb_ || cb_->strong.load(std::memory_order_acquire) == 0;
    }
    long use_count() const noexcept {
        return cb_ ? cb_->strong.load(std::memory_order_acquire) : 0;
    }

    // ===================== TODO(A2-15) lock ==========================
    // 尝试把弱引用"提升"为 SharedPtr：
    //   - 对象还活着 → 返回一个持有它的 SharedPtr（strong 已 +1）
    //   - 对象已销毁 → 返回空 SharedPtr
    //
    // 为什么必须用 CAS 循环，而不能"先判 strong>0 再 fetch_add"？
    //   因为判断和自增之间，别的线程可能恰好把 strong 减到 0 并销毁对象，
    //   你就会把一个已死对象的计数 +1 → 复活一个野指针。CAS 把"仅当 strong>0
    //   时才 +1"做成一个原子动作，杜绝这个竞态。
    //
    //   if (!cb_) return SharedPtr<T>{};
    //   long cur = cb_->strong.load(std::memory_order_relaxed);
    //   while (cur != 0) {
    //       if (cb_->strong.compare_exchange_weak(
    //               cur, cur + 1,
    //               std::memory_order_acq_rel, std::memory_order_relaxed)) {
    //           return SharedPtr<T>(ptr_, cb_);  // 用私有构造，不再 ++strong
    //       }
    //       // CAS 失败：cur 已被更新为最新值，继续循环重试
    //   }
    //   return SharedPtr<T>{};  // strong 已是 0
    // ===============================================================
    SharedPtr<T> lock() const noexcept {
        if (!cb_) return SharedPtr<T>{};
        long cur = cb_->strong.load(std::memory_order_relaxed);
        while (cur != 0) {
            if (cb_->strong.compare_exchange_weak(cur, cur + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return SharedPtr<T>(ptr_, cb_);
            }
        }
        return SharedPtr<T>{};
        // TODO
    }

private:
    // ===================== TODO(A2-14) release_weak_ =================
    // 弱引用减一；若减到 0（连"shared 合计的那 1 个"也没了），回收控制块。
    //   if (!cb_) return;
    //   if (cb_->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    //       delete cb_;
    //   }
    // ===============================================================
    void release_weak_() noexcept {
        if (!cb_) return;
        if (cb_->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete cb_;
        }
        // TODO
    }

    T* ptr_ = nullptr;
    detail::ControlBlock<T>* cb_ = nullptr;
};

} // namespace cppbc
