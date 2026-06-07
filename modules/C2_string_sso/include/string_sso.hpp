// =============================================================================
//  C2 · 手写 string（小字符串优化 SSO）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个带 SSO（Small String Optimization，小字符串优化）的简化版 string。
//  核心思想：**短字符串直接存在对象内部的小缓冲区里，不去堆上分配**；只有变长了
//  才搬到堆上。这样像 "id"、"name" 这种短串完全零堆分配，极快——这正是
//  libstdc++/libc++ 里 std::string 的关键优化，也是面试高频题。
//
//  【SSO 的存储布局（本模块用最直观的写法）】
//      char  buf_[16];     // 内联缓冲：短串(≤15字符)直接住这里，第16字节放 '\0'
//      char* data_;        // 指向"当前真正的存储"：短串时 == buf_，长串时 == 堆指针
//      size_t size_;       // 当前长度（不含 '\0'）
//      size_t capacity_;   // 当前可用容量（不含 '\0'）：短串时 == 15
//
//  判断当前是不是"短串模式"，只需看 data_ 是否指向自己的 buf_：
//      bool is_small() const { return data_ == buf_; }
//
//  （真正的标准库用 union 把 buf_ 和"堆指针+容量"叠在一起以省空间；这里用独立字段，
//    牺牲一点点内存换取代码清晰——理解了原理，union 版只是省空间的工程优化。）
//
//  【SSO 最容易踩的坑：拷贝/移动时 data_ 必须重新指向"自己的" buf_】
//  因为 buf_ 是对象内部的数组，每个对象有自己的一份。如果短串拷贝时直接复制
//  data_ 指针，新对象的 data_ 就会指向【旧对象】的 buf_ → 旧对象一析构就变野指针！
//  所以短串拷贝/移动要把内容 memcpy 进【自己的】 buf_，再让 data_ = buf_。
//  这也意味着：**短串无法"偷指针"式移动**（指针指向源对象内部），只能逐字节拷贝。
//
//  【四个底层动作】
//   - 申请堆：new char[n + 1]          （+1 给结尾 '\0'）
//   - 释放堆：delete[] data_           （仅当 !is_small() 时才释放！）
//   - 拷内容：std::memcpy(dst, src, size_ + 1)   （+1 把 '\0' 也带上）
//   - 末尾收口：data_[size_] = '\0'    （C 风格字符串必须以 '\0' 结尾）
//
// =============================================================================
#pragma once

#include <cstddef>   // std::size_t
#include <cstring>   // std::strlen, std::memcpy
#include <utility>   // std::move

namespace cppbc {

class String {
public:
    // 内联缓冲能容纳的最大字符数（不含结尾 '\0'）。
    static constexpr std::size_t kInlineCap = 15;

    // 默认构造：空串，短串模式（data_ 指向 buf_，buf_ 已清零 → c_str() 返回 ""）。
    String() noexcept = default;

    // ===================== TODO(C2-1) 从 C 字符串构造 ================
    // 用一个以 '\0' 结尾的 const char* 初始化。
    //   提示：默认成员初始化已让 data_=buf_、capacity_=kInlineCap、size_=0、buf_ 清零，
    //         所以你在函数体里"接着写"即可：
    //     std::size_t len = std::strlen(s);
    //     if (len > kInlineCap) {                 // 放不下 → 走堆
    //         data_ = new char[len + 1];
    //         capacity_ = len;
    //     }                                       // 否则 data_ 仍是 buf_，capacity_ 仍是 15
    //     std::memcpy(data_, s, len + 1);         // 连同结尾 '\0' 一起拷
    //     size_ = len;
    // ===============================================================
    String(const char* s) {
        // TODO
        (void)s;
    }

    // ===================== TODO(C2-6) 拷贝构造（深拷贝） =============
    // 复制 other 的内容。注意：短串要拷进【自己的】 buf_，长串要另开一块堆。
    //     if (other.size_ > kInlineCap) {
    //         data_ = new char[other.size_ + 1];  // 长串：独立堆内存
    //         capacity_ = other.size_;
    //     }                                       // 短串：data_ 保持指向自己的 buf_
    //     std::memcpy(data_, other.data_, other.size_ + 1);
    //     size_ = other.size_;
    // ===============================================================
    String(const String& other) {
        // TODO
        (void)other;
    }

    // ===================== TODO(C2-7) 移动构造 =======================
    // 把 other 的内容"搬"过来。SSO 的关键分支：
    //   if (other.is_small()) {
    //       // 短串：指针指向 other 内部，不能偷！只能把内容拷进自己的 buf_。
    //       std::memcpy(buf_, other.buf_, other.size_ + 1);
    //       data_ = buf_;
    //       size_ = other.size_;
    //       capacity_ = kInlineCap;
    //   } else {
    //       // 长串：直接接管堆指针（O(1)，真正的"移动"）。
    //       data_ = other.data_;
    //       size_ = other.size_;
    //       capacity_ = other.capacity_;
    //       // 把 other 复位成"空的短串"，否则它析构时会 delete 掉我们接管的堆。
    //       other.data_ = other.buf_;
    //       other.buf_[0] = '\0';
    //       other.size_ = 0;
    //       other.capacity_ = kInlineCap;
    //   }
    // ===============================================================
    String(String&& other) noexcept {
        // TODO
        (void)other;
    }

    // ===================== TODO(C2-8) 拷贝赋值 =======================
    //   1) 自赋值检查：if (this == &other) return *this;
    //   2) 释放自己当前的堆（仅当 !is_small()）：if (!is_small()) delete[] data_;
    //   3) 复位为短串：data_ = buf_; capacity_ = kInlineCap;
    //   4) 若 other 是长串，另开堆：data_ = new char[other.size_+1]; capacity_ = other.size_;
    //   5) std::memcpy(data_, other.data_, other.size_ + 1); size_ = other.size_;
    //   6) return *this;
    // ===============================================================
    String& operator=(const String& other) {
        // TODO
        (void)other;
        return *this;
    }

    // ===================== TODO(C2-9) 移动赋值 =======================
    //   1) 自赋值检查；
    //   2) 释放自己当前的堆（仅当 !is_small()）；
    //   3) 按 other 是短/长串，分别"拷进自己 buf_" 或 "接管堆指针并复位 other"
    //      （逻辑同移动构造的两个分支）；
    //   4) return *this;
    // ===============================================================
    String& operator=(String&& other) noexcept {
        // TODO
        (void)other;
        return *this;
    }

    // ===================== TODO(C2-5) 析构 ===========================
    // 仅当处于"长串模式"（数据在堆上）时才释放；短串模式下 data_ 指向 buf_，不能 delete。
    //   if (!is_small()) delete[] data_;
    // ===============================================================
    ~String() {
        // TODO
    }

    // ===================== TODO(C2-2) reserve ========================
    // 确保容量至少为 newcap；不够就搬到一块更大的堆内存上。
    //   if (newcap <= capacity_) return;
    //   char* newdata = new char[newcap + 1];
    //   std::memcpy(newdata, data_, size_ + 1);   // 连 '\0' 一起搬
    //   if (!is_small()) delete[] data_;          // 释放旧堆（旧的若是 buf_ 则不释放）
    //   data_ = newdata;
    //   capacity_ = newcap;
    //   （注意：执行完后 data_ != buf_，自动变成"长串模式"。）
    // ===============================================================
    void reserve(std::size_t newcap) {
        // TODO
        (void)newcap;
    }

    // ===================== TODO(C2-3) push_back ======================
    // 在末尾追加一个字符，维持 '\0' 收尾。
    //   if (size_ == capacity_) reserve(capacity_ == 0 ? 1 : capacity_ * 2);
    //   data_[size_] = c;
    //   data_[size_ + 1] = '\0';
    //   ++size_;
    // ===============================================================
    void push_back(char c) {
        // TODO
        (void)c;
    }

    // ===================== TODO(C2-4) append =========================
    // 在末尾拼接一个 C 字符串。
    //   std::size_t len = std::strlen(s);
    //   if (size_ + len > capacity_) reserve(size_ + len);   // 一次扩到位
    //   std::memcpy(data_ + size_, s, len + 1);              // 含 '\0'
    //   size_ += len;
    // ===============================================================
    void append(const char* s) {
        // TODO
        (void)s;
    }

    // ---- 下面是已给出的部分，无需修改 ----

    // 是否处于"短串（内联）模式"。教学用：方便测试验证 SSO 是否真的生效。
    bool is_small() const noexcept { return data_ == buf_; }

    const char* c_str() const noexcept { return data_; }
    const char* data()  const noexcept { return data_; }
    char*       data()        noexcept { return data_; }

    char&       operator[](std::size_t i)       noexcept { return data_[i]; }
    const char& operator[](std::size_t i) const noexcept { return data_[i]; }

    std::size_t size()     const noexcept { return size_; }
    std::size_t length()   const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool        empty()    const noexcept { return size_ == 0; }

    char*       begin()       noexcept { return data_; }
    char*       end()         noexcept { return data_ + size_; }
    const char* begin() const noexcept { return data_; }
    const char* end()   const noexcept { return data_ + size_; }

private:
    char        buf_[kInlineCap + 1] = {};   // 内联缓冲，默认清零（空串）
    char*       data_     = buf_;            // 短串时指向 buf_，长串时指向堆
    std::size_t size_     = 0;
    std::size_t capacity_ = kInlineCap;
};

} // namespace cppbc
