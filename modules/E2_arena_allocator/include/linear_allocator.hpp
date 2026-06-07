// =============================================================================
//  E2 · 线性/竞技场/帧分配器 LinearAllocator（bump allocator）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现游戏引擎里最快的一种分配器：**线性分配器**（又名 bump / arena / frame allocator）。
//  它预先持有一整块缓冲区，分配时只把一个"游标(offset)"往前推——快到一次分配就是
//  几条指令（对齐 + 加法）。它**不支持单独释放某个对象**，而是整块一起回收：典型用法
//  是"每帧开始 reset()，这一帧里随便 allocate 临时数据，帧末整片丢弃"。
//
//  【为什么游戏引擎要这种"不能单独释放"的分配器？】
//    游戏每帧产生大量**临时**数据：可见物列表、粒子、绘制指令、字符串拼接……它们的
//    生命周期都是"就这一帧"。用 new/delete 逐个分配/释放：慢、产生碎片、还要小心漏释放。
//    线性分配器把这些临时分配压成"游标 +="，帧末一句 reset() 全部回收——
//    **分配 O(1)、回收 O(1)、零碎片、cache 极友好**。这是 DOD/引擎里"按生命周期分配"
//    思想的代表（per-frame arena）。
//
//  【核心机关：bump（推游标）】
//      buffer_: [#############------------------------]
//                            ↑ offset_（已用到这里）
//      allocate(size, align):
//        1) 把当前位置向上对齐到 align     （地址必须对齐，否则放 double/SIMD 会崩/变慢）
//        2) 若对齐后 + size 超出 capacity_ → 返回 nullptr（这片用满了）
//        3) 把 offset_ 推到末尾，返回对齐后的地址
//      reset():  offset_ = 0               （整片作废，O(1)）
//
//  【进阶：marker / rewind —— 栈式作用域回收】
//    除了"整片 reset"，还能"回退到某个历史游标"：进入一段临时计算前存一个 marker()，
//    算完 rewind(marker) 把这段用掉的内存一次性吐回去（后进先出，像栈帧）。这让同一片
//    arena 支持嵌套的临时作用域，而不必等到帧末才整片重置。
//
//  【和定长内存池(C3)的区别 —— 面试常对比】
//    · C3 定长池：能**逐个** deallocate、块大小固定、用空闲链表复用空块。
//    · E2 线性池：**不能**逐个释放、可要任意大小/对齐、只能整片 reset/rewind；换来更极致的速度。
//    两者互补：长生命周期的同类对象用池；一帧即弃的杂项用线性分配器。
//
//  【只给生内存，对象生命周期靠你】
//    allocate() 返回**未构造**的生内存。要造对象用 placement new（create<T> 已给好示范）。
//    注意：reset/rewind **不会**帮你调析构函数！若在 arena 上构造了非平凡类型（持有资源/
//    需要析构），你得在回收前自己 p->~T()。对"纯数据(POD)"则可直接丢弃，无需析构。
//
//  【你会实现的 3 个 TODO】（align_up / create / 观测接口都已给好）
//    E2-1  allocate —— 对齐当前游标、查容量、推游标、返回对齐地址（满了返回 nullptr）
//    E2-2  reset    —— 游标归零，整片作废
//    E2-3  rewind   —— 游标回退到给定 marker（栈式回收）
//
// =============================================================================
#pragma once

#include <cstddef>  // std::size_t, std::byte, std::max_align_t
#include <cstdint>  // std::uintptr_t
#include <memory>   // std::unique_ptr, std::make_unique
#include <new>      // placement new
#include <utility>  // std::forward

namespace cppbc {

// ---------------------------------------------------------------------------
//  LinearAllocator：持有一整片缓冲区，分配=推游标，回收=整片 reset / 栈式 rewind。
// ---------------------------------------------------------------------------
class LinearAllocator {
    std::unique_ptr<std::byte[]> buffer_;        // 持有的整片缓冲区（析构时自动释放）
    std::size_t                  capacity_ = 0;  // 缓冲区总字节数
    std::size_t                  offset_   = 0;  // 游标：已用字节数（从 buffer_ 起算）

    // 把地址/偏移 n 向上取整到 align 的倍数（align 必须是 2 的幂）。已给好。
    static std::uintptr_t align_up(std::uintptr_t n, std::size_t align) noexcept {
        return (n + (align - 1)) & ~(static_cast<std::uintptr_t>(align) - 1);
    }

public:
    using Marker = std::size_t;  // 游标快照类型（就是某一刻的 offset_）

    explicit LinearAllocator(std::size_t capacity)
        : buffer_(std::make_unique<std::byte[]>(capacity)), capacity_(capacity) {}

    // 持有唯一缓冲区：禁拷贝，允许移动。
    LinearAllocator(const LinearAllocator&)            = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) noexcept            = default;
    LinearAllocator& operator=(LinearAllocator&&) noexcept = default;

    // ===================== TODO(E2-1) allocate ======================
    //  从缓冲区切出 size 字节、满足 align 对齐的生内存；这一片用满则返回 nullptr。
    //    std::uintptr_t base    = reinterpret_cast<std::uintptr_t>(buffer_.get());
    //    std::uintptr_t aligned = align_up(base + offset_, align);  // 对齐"绝对地址"
    //    std::size_t    needed  = static_cast<std::size_t>(aligned - base) + size;
    //    if (needed > capacity_) return nullptr;     // 容量不够 → 分配失败，游标保持不动
    //    offset_ = needed;                           // 推游标到本次分配末尾
    //    return reinterpret_cast<void*>(aligned);
    // ===============================================================
    void* allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) {
        // TODO
        (void)size; (void)align;
        return nullptr;   // 骨架：恒分配失败
    }

    // ===================== TODO(E2-2) reset =========================
    //  整片作废：游标归零，下次分配又从头开始。（注意：不调用任何析构函数！）
    //    offset_ = 0;
    // ===============================================================
    void reset() noexcept {
        // TODO
    }

    // ===================== TODO(E2-3) rewind ========================
    //  回退到某个历史游标 m（marker 取得），把其后分配的内存一次性回收（后进先出）。
    //    offset_ = m;
    // ===============================================================
    void rewind(Marker m) noexcept {
        // TODO
        (void)m;
    }

    // 记录当前游标，配合 rewind 实现栈式作用域回收。已给好。
    Marker marker() const noexcept { return offset_; }

    // 便捷：在 arena 上构造一个对象并返回指针（失败返回 nullptr）。已给好。
    //  注意：reset/rewind 不会析构它；非平凡类型需你自己在回收前 p->~T()。
    template <class T, class... Args>
    T* create(Args&&... args) {
        void* p = allocate(sizeof(T), alignof(T));
        if (!p) return nullptr;
        return new (p) T(std::forward<Args>(args)...);
    }

    std::size_t used()     const noexcept { return offset_; }    // 已用字节
    std::size_t capacity() const noexcept { return capacity_; }  // 总容量
};

} // namespace cppbc
