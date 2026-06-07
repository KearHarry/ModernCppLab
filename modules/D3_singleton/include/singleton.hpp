// =============================================================================
//  D3 · 线程安全的单例（Meyers / call_once / 双重检查锁 DCLP）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  单例（Singleton）= 全局只允许存在「唯一一个」实例，并提供一个全局访问点。
//  日志器、配置中心、连接池、线程池常做成单例。本模块的重点不是"单例好不好"，
//  而是 **多线程同时第一次访问时，如何保证构造恰好发生一次、且大家拿到同一个对象**。
//  这是并发面试的高频考点，能把三种正确写法讲清楚就赢了一半。
//
//  【为什么"懒汉式"单例在多线程下会出错？】
//  最朴素的懒汉式：
//      if (instance_ == nullptr)       // ① 检查
//          instance_ = new T();        // ② 构造并赋值
//      return instance_;
//  两个线程可能同时通过 ① 的检查，于是各 new 一次 → 构造了两个对象、内存泄漏、
//  甚至返回不同实例。这是典型的「检查-再-执行（check-then-act）」竞态。
//
//  【三种正确写法，本模块各实现一个 instance()】
//
//   ① Meyers 单例（最推荐）—— 函数内 static 局部变量：
//        static T inst;            // C++11 起，标准保证它的初始化是线程安全的
//      编译器自动加同步：并发首次访问时只有一个线程执行构造，其余线程等它完成。
//      代码最短、最快、最安全，现代 C++ 首选。俗称 "magic statics"。
//
//   ② std::call_once + std::once_flag：
//        std::call_once(flag, []{ instance_ = new T(); });
//      标准库保证：传给 call_once 的可调用物，在多线程并发下也**恰好执行一次**。
//      适合"初始化逻辑较复杂、不止一行"的场景。
//
//   ③ 双重检查锁 DCLP（Double-Checked Locking Pattern）—— 手写同步，理解原理用：
//        T* p = instance_.load(acquire);        // 第一次检查（无锁，快路径）
//        if (!p) {
//            lock_guard lk(mtx_);
//            p = instance_.load(relaxed);        // 第二次检查（持锁，防重复构造）
//            if (!p) { p = new T(); instance_.store(p, release); }
//        }
//        return *p;
//      历史教训：C++11 之前没有内存模型，DCLP 因「指针可见 ≠ 对象已构造完成」的
//      重排序问题而**著名地不可靠**。C++11 引入 std::atomic 与 acquire/release 内存序
//      后才有了可移植的正确写法——store 用 release 发布，load 用 acquire 接收，
//      确保"看到非空指针"的线程也必然看到"构造完成的对象"。
//
//  （三个类各自用一个 inline static 原子计数器 ctor_count 记录构造次数，
//    供测试断言"构造恰好一次"。私有构造函数里给它 +1。）
//
// =============================================================================
#pragma once

#include <atomic>   // std::atomic
#include <cstddef>  // std::size_t
#include <mutex>    // std::mutex, std::once_flag, std::call_once, std::lock_guard

namespace cppbc {

// ---------------------------------------------------------------------------
//  ① Meyers 单例：函数内 static 局部变量（C++11 保证线程安全初始化）
// ---------------------------------------------------------------------------
class MeyersSingleton {
public:
    // 构造次数计数器（原子）。正确实现下，全程只应为 1。
    inline static std::atomic<int> ctor_count{0};

    // ===================== TODO(D3-1) Meyers instance() ==============
    //   static MeyersSingleton inst;   // 关键：函数内 static，C++11 起线程安全初始化
    //   return inst;
    // ===============================================================
    static MeyersSingleton& instance() {
        // TODO：骨架先"每次都新建一个"——这并不是单例（地址不同、计数>1），测试会红。
        return *new MeyersSingleton();
    }

    void set(int v) noexcept { value_ = v; }
    int  get() const noexcept { return value_; }

    MeyersSingleton(const MeyersSingleton&)            = delete;
    MeyersSingleton& operator=(const MeyersSingleton&) = delete;

private:
    MeyersSingleton() { ctor_count.fetch_add(1, std::memory_order_relaxed); }
    int value_ = 0;
};

// ---------------------------------------------------------------------------
//  ② call_once 单例：std::call_once 保证初始化逻辑恰好执行一次
// ---------------------------------------------------------------------------
class CallOnceSingleton {
public:
    inline static std::atomic<int> ctor_count{0};

    // ===================== TODO(D3-2) call_once instance() ==========
    //   std::call_once(once_, [] { instance_ = new CallOnceSingleton(); });
    //   return *instance_;
    // ===============================================================
    static CallOnceSingleton& instance() {
        // TODO：骨架先"每次都新建一个"——测试会红。
        return *new CallOnceSingleton();
    }

    void set(int v) noexcept { value_ = v; }
    int  get() const noexcept { return value_; }

    CallOnceSingleton(const CallOnceSingleton&)            = delete;
    CallOnceSingleton& operator=(const CallOnceSingleton&) = delete;

private:
    CallOnceSingleton() { ctor_count.fetch_add(1, std::memory_order_relaxed); }
    int value_ = 0;

    inline static std::once_flag       once_;
    inline static CallOnceSingleton*   instance_ = nullptr;
};

// ---------------------------------------------------------------------------
//  ③ DCLP 单例：双重检查锁 + 原子指针 + acquire/release 内存序
// ---------------------------------------------------------------------------
class DclpSingleton {
public:
    inline static std::atomic<int> ctor_count{0};

    // ===================== TODO(D3-3) DCLP instance() ===============
    //   DclpSingleton* p = instance_.load(std::memory_order_acquire);  // 快路径，无锁
    //   if (!p) {
    //       std::lock_guard<std::mutex> lk(mtx_);
    //       p = instance_.load(std::memory_order_relaxed);             // 持锁再查一次
    //       if (!p) {
    //           p = new DclpSingleton();
    //           instance_.store(p, std::memory_order_release);         // release 发布
    //       }
    //   }
    //   return *p;
    // ===============================================================
    static DclpSingleton& instance() {
        // TODO：骨架先"每次都新建一个"——测试会红。
        return *new DclpSingleton();
    }

    void set(int v) noexcept { value_ = v; }
    int  get() const noexcept { return value_; }

    DclpSingleton(const DclpSingleton&)            = delete;
    DclpSingleton& operator=(const DclpSingleton&) = delete;

private:
    DclpSingleton() { ctor_count.fetch_add(1, std::memory_order_relaxed); }
    int value_ = 0;

    inline static std::atomic<DclpSingleton*> instance_{nullptr};
    inline static std::mutex                  mtx_;
};

} // namespace cppbc
