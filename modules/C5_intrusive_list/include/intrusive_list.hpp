// =============================================================================
//  C5 · 侵入式双向链表 IntrusiveList（intrusive linked list）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一种"把链表指针塞进对象自己身上"的链表。普通 std::list<T> 每放一个元素，都要在堆上
//  额外 new 一个含 prev/next 的节点把 T 包起来——多一次分配、多一层 cache 不命中。
//  **侵入式链表**反过来：让 T 自带 prev/next（继承一个 ListNode 钩子），对象本身就是节点。
//  于是入链/出链**零额外分配**；而且——只要拿到对象本身，就能 O(1) 把它从链表摘下来，
//  不用先去链表里搜一遍。Linux 内核 list_head、boost.intrusive、以及几乎所有游戏引擎的
//  对象管理（一个实体同时挂在"可见列表""物理列表""待删除列表"里）都用它。
//
//  【和 std::list 的根本区别】
//    std::list<Widget>:  额外 [node:prev|next|→Widget]  ← 每元素一次堆分配，节点与对象分离
//    IntrusiveList:      Widget 自己 = [prev|next | ...Widget数据...]  ← 对象即节点，零分配
//
//  【侵入式的三大超能力（面试考点）】
//    1) 零分配入链：对象已存在，链接只是改几个指针，不 new 节点。
//    2) O(1) 摘除：unlink(对象) 只需该对象的 prev/next，无需遍历查找它在哪
//       （std::list 要 erase 得先持有 iterator；裸数组要 remove 得先 find）。
//    3) 多链表成员：一个对象带多个钩子，就能同时挂在多个链表里（各用各的 prev/next）。
//
//  【数据结构：带哨兵的环形双向链表】
//    head_ 是个"哨兵"节点（不放数据），首尾相接成环：
//        head_ <-> A <-> B <-> C <-> head_
//    空链表时 head_.next == head_.prev == &head_（自环）。哨兵让插入/删除无需特判头尾，
//    代码干净——这是工业级链表的标准技巧。front = head_.next，back = head_.prev。
//
//  【从钩子指针找回对象：static_cast 向下转型】
//    T 公有继承 ListNode，所以 ListNode* 指向的其实是某个 T 的基类子对象。
//    static_cast<T*>(node) 安全地转回去（对象本就是 T，编译器自动算好基类偏移）。
//    哨兵 head_ 是裸 ListNode、永远不转成 T——遍历到它就停。
//
//  【你会实现的 3 个 TODO】（哨兵构造、empty/front/back/size/for_each 都已给好）
//    C5-1  push_back —— 把对象的节点接到尾部（哨兵之前）
//    C5-2  unlink    —— 把对象从所在链表摘除（纯指针手术，O(1)，这是核心超能力）
//    C5-3  pop_front —— 摘下并返回首元素（空则 nullptr），复用 unlink
//
//  ⚠ 侵入式链表【不拥有】元素：它从不 new/delete 对象，只改指针。对象生命周期由你管
//     （常是栈对象、对象池里的对象、或 slot map 里的对象）。链表析构不会删元素。
//
// =============================================================================
#pragma once

#include <cstddef>  // std::size_t

namespace cppbc {

// 链表钩子：让对象自带前后指针。用户类型公有继承它即可入链。
struct ListNode {
    ListNode* prev = nullptr;
    ListNode* next = nullptr;
};

// ---------------------------------------------------------------------------
//  IntrusiveList<T>：要求 T 公有继承 ListNode。带哨兵的环形双向链表。
// ---------------------------------------------------------------------------
template <class T>
class IntrusiveList {
    ListNode head_;  // 哨兵：head_.next=首元素、head_.prev=尾元素；空链时自环

public:
    IntrusiveList() noexcept {
        head_.next = &head_;   // 初始自环 = 空链表
        head_.prev = &head_;
    }

    // 不可拷贝：链接关系不该被复制（两个链表共享同一批节点会乱套）。
    IntrusiveList(const IntrusiveList&)            = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;

    bool empty() const noexcept { return head_.next == &head_; }

    // 元素个数：遍历计数 O(n)（侵入式链表通常不另维护计数器）。已给好。
    std::size_t size() const noexcept {
        std::size_t n = 0;
        for (const ListNode* p = head_.next; p != &head_; p = p->next) ++n;
        return n;
    }

    T* front() noexcept { return empty() ? nullptr : static_cast<T*>(head_.next); }
    T* back()  noexcept { return empty() ? nullptr : static_cast<T*>(head_.prev); }

    // 顺序遍历，对每个元素调用 f(T&)。已给好（注意：绝不把哨兵 head_ 转成 T）。
    template <class F>
    void for_each(F f) {
        for (ListNode* p = head_.next; p != &head_; p = p->next) {
            f(static_cast<T&>(*p));
        }
    }

    // ===================== TODO(C5-1) push_back =====================
    //  把对象 obj 的节点接到链尾（哨兵 head_ 之前）。
    //    ListNode* n    = &static_cast<ListNode&>(obj);  // 上转型拿到 obj 的钩子
    //    ListNode* last = head_.prev;                    // 原来的尾
    //    n->prev    = last;
    //    n->next    = &head_;
    //    last->next = n;
    //    head_.prev = n;
    // ===============================================================
    void push_back(T& obj) {
        ListNode* n = &static_cast<ListNode&>(obj);
        ListNode* last = head_.prev;
        n->prev = last;
        n->next = &head_;
        last->next = n;
        head_.prev = n;
    }

    // ===================== TODO(C5-2) unlink ========================
    //  把对象从它所在的链表摘除——只用它自己的 prev/next，O(1)，无需遍历。
    //  这是侵入式链表的核心超能力。前置条件：obj 当前确实在某个链表里。
    //    ListNode* n = &static_cast<ListNode&>(obj);
    //    n->prev->next = n->next;     // 让前驱跳过我
    //    n->next->prev = n->prev;     // 让后继跳过我
    //    n->prev = nullptr;           // 标记为"已脱链"
    //    n->next = nullptr;
    // ===============================================================
    static void unlink(T& obj) {
        ListNode* n = &static_cast<ListNode&>(obj);
        n->prev->next = n->next;
        n->next->prev = n->prev;
        n->prev = nullptr;
        n->next = nullptr;
    }

    // ===================== TODO(C5-3) pop_front =====================
    //  摘下并返回首元素指针；空链表返回 nullptr。复用 unlink。
    //    if (empty()) return nullptr;
    //    ListNode* first = head_.next;
    //    unlink(static_cast<T&>(*first));
    //    return static_cast<T*>(first);
    // ===============================================================
    T* pop_front() {
        if (empty()) return nullptr;
        ListNode* first = head_.next;
        unlink(static_cast<T&>(*first));
        return static_cast<T*>(first);
    }
};

} // namespace cppbc
