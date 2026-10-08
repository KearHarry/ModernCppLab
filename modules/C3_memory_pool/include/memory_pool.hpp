// =============================================================================
//  C3 · 定长对象内存池 / free-list 分配器（memory pool）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个**固定块大小**的内存池 `FixedPool`：预先向系统批发一大块内存，切成等大的
//  小块用**空闲链表(free list)** 串起来；`allocate()` 从链表头摘一块、`deallocate()`
//  把块还回链表头——都是 **O(1)** 且不碰系统分配器。大厂常考"手写内存池/对象池"，
//  它是高性能服务器、游戏引擎、STL allocator 的基础，也是理解"为什么频繁 new/delete 慢"
//  的最佳切入点。
//
//  【为什么需要内存池？new/delete 慢在哪？】
//  通用 `new/delete`（底层 malloc/free）要应付**任意大小**的请求：维护复杂的空闲结构、
//  处理碎片、常常还要加锁（多线程安全）。如果你的程序疯狂创建/销毁**同一种小对象**
//  （网络包、节点、粒子……），每次都走通用分配器就很浪费。内存池针对"**大小固定**"
//  这一前提把问题简化到极致：
//    · 分配 = 摘下链表头节点（几条指令），无搜索、无加锁（单线程版）。
//    · 释放 = 把节点插回链表头（几条指令）。
//    · 内存**批发**（一次要一大片 chunk），摊薄系统调用开销，也减少碎片。
//
//  【核心数据结构：侵入式空闲链表(intrusive free list)】
//  关键技巧——**空闲块自己存"下一个空闲块"的指针**。块在"空闲"时反正没人用它的内容，
//  正好借这块内存的前 8 字节当 `next` 指针，把所有空闲块串成单链表。这样**不需要额外的
//  管理数组**，元数据"寄生"在空闲块自身里（所以块大小必须 ≥ 一个指针大小）。
//
//      free_list_ ─▶ [blk] ─▶ [blk] ─▶ [blk] ─▶ null
//                     ↑ 每个空闲块的前 8 字节存着指向下一个空闲块的指针
//
//      allocate(): p = free_list_; free_list_ = p->next; return p;   // 摘头
//      deallocate(p): p->next = free_list_; free_list_ = p;          // 插头（后进先出）
//
//  【内存从哪来？——按 chunk 批发，按需扩容】
//  池一开始是空的。第一次 allocate 发现 free_list_ 为空，就 `grow_()`：向系统要一大片
//  `block_size_ × blocks_per_chunk_` 字节的 **chunk**，把它切成 `blocks_per_chunk_` 个小块
//  逐个挂上 free_list_。chunk 用 `unique_ptr<byte[]>` 持有、存进 `chunks_` 向量，
//  池析构时统一释放（块本身不单独 delete，整片 chunk 一起还给系统）。
//
//      grow_() 之后：
//      chunks_ = [ ┌─────────────────────────────┐ ]   一整片 chunk
//                  │ blk0 │ blk1 │ blk2 │ blk3 │   │   切成 4 块全部挂进 free_list_
//                  └─────────────────────────────┘
//
//  【对齐(alignment)】
//  返回的内存必须满足对齐要求（否则放 double/指针可能崩或变慢）。本模块在构造时把
//  块大小**向上取整**到 `alignof(std::max_align_t)` 的倍数，且 `new byte[]` 返回的 chunk
//  起始地址本身是最大对齐的——于是每个块起点都对齐。这步已在构造函数里给好。
//
//  【和 STL allocator / placement new 的关系】
//  `allocate()` 只负责"给一块**生内存**"，不构造对象；要在这块内存上造对象得用
//  **placement new**（见 C1 vector）：`T* p = new (pool.allocate()) T(args...);`，
//  销毁时先 `p->~T()` 再 `pool.deallocate(p)`。把本池套上 allocator 接口，就能喂给
//  `std::vector` / `std::list` 当自定义分配器——这正是 STL allocator 的真实工作方式。
//
//  【你会实现的 3 个 TODO】
//    C3-1  allocate   —— 空了就 grow_，然后摘下 free_list_ 头节点
//    C3-2  deallocate —— 把归还的块插回 free_list_ 头
//    C3-3  grow_      —— 批发一片 chunk，切块并全部挂进 free_list_
//
// =============================================================================
#pragma once

#include <algorithm>  // std::max
#include <cstddef>    // std::size_t / std::byte / std::max_align_t
#include <memory>     // std::unique_ptr / make_unique
#include <vector>     // std::vector

namespace cppbc {

// ---------------------------------------------------------------------------
//  FixedPool：所有分配出去的块大小都相同（构造时确定）。
// ---------------------------------------------------------------------------
class FixedPool {
    // 空闲块借自身内存存放的"下一个空闲块"指针（侵入式链表节点）。
    struct FreeNode {
        FreeNode* next;
    };

    // 把 n 向上取整到 a 的倍数（a 必须是 2 的幂或普通正整数；这里用通用写法）。
    static constexpr std::size_t align_up(std::size_t n, std::size_t a) {
        return (n + a - 1) / a * a;
    }

    std::size_t block_size_;        // 每块字节数（≥ 指针大小，且已对齐）
    std::size_t blocks_per_chunk_;  // 每次扩容批发多少块
    FreeNode*   free_list_ = nullptr;                       // 空闲块单链表头
    std::vector<std::unique_ptr<std::byte[]>> chunks_;      // 拥有所有批发来的大块内存
    std::size_t free_count_   = 0;  // 当前空闲链表上的块数（观测/测试用）
    std::size_t outstanding_  = 0;  // 已分配出去、尚未归还的块数（观测/测试用）

    // ===================== TODO(C3-3) grow_ =========================
    //  批发一片新 chunk 并切块挂进 free_list_：
    //    auto chunk = std::make_unique<std::byte[]>(block_size_ * blocks_per_chunk_);
    //    std::byte* base = chunk.get();
    //    for (std::size_t i = 0; i < blocks_per_chunk_; ++i) {
    //        auto* node = reinterpret_cast<FreeNode*>(base + i * block_size_);
    //        node->next = free_list_;       // 头插：把每个新块挂到链表头
    //        free_list_ = node;
    //    }
    //    chunks_.push_back(std::move(chunk));  // chunk 交给 vector 持有（析构时统一释放）
    //    free_count_ += blocks_per_chunk_;
    // ===============================================================
    void grow_() {
        auto chunk = std::make_unique<std::byte[]>(block_size_ * blocks_per_chunk_);
        std::byte* base = chunk.get();
        for ( std::size_t i = 0; i < blocks_per_chunk_; ++i) {
            auto* node = reinterpret_cast<FreeNode*>(base + i * block_size_);
            node->next = free_list_;
            free_list_ = node;
        }
        chunks_.push_back(std::move(chunk));
        free_count_ += blocks_per_chunk_;
    }

public:
    // block_size：单个对象大小（会被抬到至少一个指针大小、并对齐）；
    // blocks_per_chunk：每次扩容批发的块数（默认 64）。
    explicit FixedPool(std::size_t block_size, std::size_t blocks_per_chunk = 64)
        : block_size_(align_up(std::max(block_size, sizeof(FreeNode)),
                               alignof(std::max_align_t))),
          blocks_per_chunk_(blocks_per_chunk == 0 ? 1 : blocks_per_chunk) {}

    // 内存池不可拷贝（拷贝会让两个池共享 chunk，归属混乱）。
    FixedPool(const FixedPool&)            = delete;
    FixedPool& operator=(const FixedPool&) = delete;

    // ===================== TODO(C3-1) allocate ======================
    //  取一块生内存（O(1)）：空闲链表为空时先扩容，再摘下表头。
    //    if (!free_list_) grow_();
    //    FreeNode* node = free_list_;
    //    free_list_ = node->next;
    //    --free_count_;
    //    ++outstanding_;
    //    return node;            // 当作 void* 返回（调用方再 placement new 构造对象）
    // ===============================================================
    void* allocate() {
        if(!free_list_) grow_();
        FreeNode* node = free_list_;
        free_list_ = node->next;
        --free_count_;
        ++outstanding_;
        return node;    
    }

    // ===================== TODO(C3-2) deallocate ====================
    //  归还一块内存（O(1)）：把它头插回空闲链表（不还给系统）。
    //    if (!p) return;
    //    auto* node = static_cast<FreeNode*>(p);
    //    node->next = free_list_;
    //    free_list_ = node;
    //    ++free_count_;
    //    --outstanding_;
    // ===============================================================
    void deallocate(void* p) {
        if(!p) return;
        auto* node = static_cast<FreeNode*>(p);
        node->next = free_list_;
        free_list_ = node;
        ++free_count_;
        --outstanding_;
    }

    // ---- 以下为观测接口（已给好，测试用来核对计数）----
    std::size_t block_size()  const noexcept { return block_size_; }   // 实际块大小（对齐后）
    std::size_t outstanding() const noexcept { return outstanding_; }  // 已借出未归还的块数
    std::size_t free_count()  const noexcept { return free_count_; }   // 空闲链表上的块数
    std::size_t chunk_count() const noexcept { return chunks_.size(); }// 批发过几片 chunk
};

} // namespace cppbc
