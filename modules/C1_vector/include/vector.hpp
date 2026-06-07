// =============================================================================
//  C1 · 手写 vector —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个简化版 std::vector<T>：一段会自动增长的连续内存。理解它 = 理解
//  "动态数组"如何在底层管理内存、如何摊还扩容、如何正确地构造/析构每个元素。
//
//  【最关键的一个认知：分配内存 ≠ 构造对象】
//  vector 里 capacity（容量）通常大于 size（元素个数）。多出来的那部分是
//  "生内存"（raw memory）——只分配了字节，还【没有】在上面构造对象。所以：
//    - 不能用 new T[n]（它会把 n 个都默认构造，且无法只构造一部分）。
//    - 要用 ::operator new 只【分配字节】，再用 placement new 在指定位置【构造】，
//      用显式析构 p->~T() 单独【析构】，最后用 ::operator delete 【释放字节】。
//
//  【四个底层动作（务必分清）】
//    1) 分配：  ::operator new(n * sizeof(T))        // 只要字节，不构造
//    2) 构造：  new (ptr) T(args...)                 // placement new：在 ptr 处构造
//    3) 析构：  ptr->~T()                            // 只析构，不释放字节
//    4) 释放：  ::operator delete(ptr)               // 只还字节，不析构
//
//  【扩容（reallocation）发生了什么？】
//  size == capacity 时再 push_back，要：申请更大的新内存 → 把旧元素逐个【搬】到
//  新内存（move/copy 构造）→ 析构旧元素 → 释放旧内存 → 用新内存。容量一般翻倍，
//  保证 push_back 的【摊还】复杂度是 O(1)。
//
//  【为什么搬元素用 std::move_if_noexcept？】
//  扩容搬运到一半若抛异常，旧数据可能已被破坏 → 违反"强异常保证"。move_if_noexcept
//  的规则：T 的移动构造是 noexcept 就移动（快）；否则退化为拷贝（慢但安全，拷贝
//  不破坏源）。这就是为什么"给移动构造加 noexcept"能让 vector 扩容更快——面试高频。
//
//  【Rule of Five】管理裸资源的类要完整写出：析构、拷贝构造、移动构造、
//  拷贝赋值、移动赋值。本模块逐个让你实现，体会深拷贝 vs 浅拷贝、移动后置空。
//
// =============================================================================
#pragma once

#include <cstddef>     // std::size_t
#include <new>         // placement new, ::operator new/delete
#include <stdexcept>   // std::out_of_range
#include <utility>     // std::move, std::move_if_noexcept, std::swap

namespace cppbc {

template <class T>
class Vector {
public:
    using value_type = T;
    using size_type  = std::size_t;
    using iterator   = T*;
    using const_iterator = const T*;

    // 默认构造：空 vector，不分配任何内存。
    Vector() noexcept = default;

    // ===================== TODO(C1-6) 拷贝构造（深拷贝） ==============
    // 复制 other 的全部元素到一块【新】内存里（不能共享指针，否则会双重释放）。
    //   步骤：
    //     1) 若 other 为空(size_==0)，保持空即可，直接 return。
    //     2) data_ = allocate_(other.size_);   // 申请恰好够用的生内存
    //     3) for i in [0, other.size_): new (data_ + i) T(other.data_[i]);  // 逐个拷贝构造
    //     4) size_ = capacity_ = other.size_;
    //   若第 3 步某次拷贝构造抛异常，严谨做法要析构已构造的并释放——进阶可加 try/catch，
    //   这里先按"拷贝构造不抛"处理，理解主线即可。
    // ===============================================================
    Vector(const Vector& other) {
        // TODO
        (void)other;
    }

    // ===================== TODO(C1-7) 移动构造 =======================
    // "偷"走 other 的内存：直接接管指针与计数，再把 other 置为空壳。
    //   data_ = other.data_;  size_ = other.size_;  capacity_ = other.capacity_;
    //   other.data_ = nullptr;  other.size_ = 0;  other.capacity_ = 0;
    //   （移动不分配、不拷贝元素，O(1)；别忘了把源置空，否则析构会重复释放。）
    // ===============================================================
    Vector(Vector&& other) noexcept {
        // TODO
        (void)other;
    }

    // ===================== TODO(C1-8) 拷贝赋值（copy-and-swap） ========
    // 推荐用"拷贝并交换"惯用法，天然异常安全、自动处理自赋值：
    //   if (this != &other) {
    //       Vector tmp(other);   // 复用拷贝构造做一份副本（失败也不影响 *this）
    //       swap(tmp);           // 与副本交换内脏；旧内脏随 tmp 析构而释放
    //   }
    //   return *this;
    // ===============================================================
    Vector& operator=(const Vector& other) {
        // TODO
        (void)other;
        return *this;
    }

    // ===================== TODO(C1-9) 移动赋值 =======================
    //   1) 自赋值检查：if (this == &other) return *this;
    //   2) 释放自己当前的内容：先析构所有元素（同 clear()），再 deallocate_(data_)。
    //   3) 接管 other 的指针与计数，并把 other 置空。
    //   4) return *this;
    // ===============================================================
    Vector& operator=(Vector&& other) noexcept {
        // TODO
        (void)other;
        return *this;
    }

    // ===================== TODO(C1-5) 析构 ===========================
    // 先逐个析构所有已构造的元素，再释放那块字节内存。
    //   for i in [0, size_): data_[i].~T();
    //   deallocate_(data_);
    //   （顺序很重要：必须先析构对象，再还内存。）
    // ===============================================================
    ~Vector() {
        // TODO
    }

    // ===================== TODO(C1-1) reserve ========================
    // 确保容量至少为 newcap；若本就够大则什么都不做。这是扩容的核心。
    //   if (newcap <= capacity_) return;
    //   T* newdata = allocate_(newcap);                 // 1) 申请更大的生内存
    //   for i in [0, size_):                            // 2) 把旧元素搬到新内存
    //       new (newdata + i) T(std::move_if_noexcept(data_[i]));
    //   for i in [0, size_): data_[i].~T();             // 3) 析构旧元素
    //   deallocate_(data_);                             // 4) 释放旧内存
    //   data_ = newdata;                                // 5) 换上新内存
    //   capacity_ = newcap;
    //   （size_ 不变——元素个数没变，只是搬了家。）
    // ===============================================================
    void reserve(size_type newcap) {
        // TODO
        (void)newcap;
    }

    // ===================== TODO(C1-2) push_back ======================
    // 在末尾追加一个元素。两个重载：左值版拷贝、右值版移动。
    //   通用步骤：
    //     1) 若 size_ == capacity_ 满了，先扩容：
    //          reserve(capacity_ == 0 ? 1 : capacity_ * 2);   // 翻倍增长
    //     2) 在末尾的生内存上 placement new 构造元素：
    //          左值版：new (data_ + size_) T(value);
    //          右值版：new (data_ + size_) T(std::move(value));
    //     3) ++size_;
    //   想一想：为什么是"翻倍"而不是"+1"？（摊还分析：+1 会让 n 次 push 变成 O(n^2)）
    // ===============================================================
    void push_back(const T& value) {
        // TODO
        (void)value;
    }
    void push_back(T&& value) {
        // TODO
        (void)value;
    }

    // ===================== TODO(C1-3) pop_back =======================
    // 删除末尾元素：只需析构它并把 size_ 减一（容量不变，内存不还）。
    //   data_[size_ - 1].~T();
    //   --size_;
    //   （调用者需保证非空；这里不做检查，和标准库一致。）
    // ===============================================================
    void pop_back() {
        // TODO
    }

    // ===================== TODO(C1-4) clear ==========================
    // 清空：析构所有元素，size_ 归 0，但【保留】已分配的容量（不还内存）。
    //   for i in [0, size_): data_[i].~T();
    //   size_ = 0;
    // ===============================================================
    void clear() noexcept {
        // TODO
    }

    // ---- 下面是已给出的部分，无需修改 ----

    // 交换两个 vector 的内脏（O(1)，被 copy-and-swap 复用）。
    void swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // 下标访问（不做边界检查，和标准库 operator[] 一致）。
    T&       operator[](size_type i)       noexcept { return data_[i]; }
    const T& operator[](size_type i) const noexcept { return data_[i]; }

    // 带边界检查的访问：越界抛 std::out_of_range（和标准库 at() 一致）。
    T& at(size_type i) {
        if (i >= size_) throw std::out_of_range("Vector::at: index out of range");
        return data_[i];
    }
    const T& at(size_type i) const {
        if (i >= size_) throw std::out_of_range("Vector::at: index out of range");
        return data_[i];
    }

    size_type size()     const noexcept { return size_; }
    size_type capacity() const noexcept { return capacity_; }
    bool      empty()    const noexcept { return size_ == 0; }

    T*       data()        noexcept { return data_; }
    const T* data()  const noexcept { return data_; }
    iterator begin()       noexcept { return data_; }
    iterator end()         noexcept { return data_ + size_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator end()   const noexcept { return data_ + size_; }

private:
    // 只【分配字节】，不构造对象。n==0 时返回 nullptr。
    static T* allocate_(size_type n) {
        if (n == 0) return nullptr;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    // 只【释放字节】，不析构对象（析构已在别处单独做过）。
    static void deallocate_(T* p) noexcept {
        ::operator delete(p);
    }

    T*        data_     = nullptr;
    size_type size_     = 0;
    size_type capacity_ = 0;
};

// 非成员 swap（让 std::swap(a, b) 走我们的高效版本）。
template <class T>
void swap(Vector<T>& a, Vector<T>& b) noexcept {
    a.swap(b);
}

} // namespace cppbc
