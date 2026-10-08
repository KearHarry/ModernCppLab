// =============================================================================
//  B4 · 无锁栈 Treiber Stack 与 ABA 问题（lock-free / CAS）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个**不用锁**的并发栈：多个线程同时 push/pop，全靠一条原子指令 **CAS**
//  （compare-and-swap，比较并交换）来协调，而不是 mutex。这是最经典的无锁数据结构
//  ——**Treiber 栈**。大厂并发岗几乎必问"无锁怎么做""CAS 是什么""ABA 是什么"。
//  本模块承接 B3（原子量与内存序），把 CAS 真正用起来造出一个能用的并发容器。
//
//  【为什么要无锁？锁不好吗？】
//  mutex 能保证正确，但有代价：线程抢不到锁会被操作系统**挂起/唤醒**（上下文切换很贵）；
//  持锁线程若被抢占，其他线程只能干等（优先级反转、护航效应）。**无锁(lock-free)** 用
//  原子指令让线程"各自不停重试"，不会有人被无限期挂起——吞吐更稳、更抗抖动。代价是
//  实现极其烧脑、容易出 ABA / 内存回收等隐蔽 bug。这正是它"显水平"、被爱考的原因。
//
//  【核心武器：CAS（compare_exchange）】
//  `head_.compare_exchange_weak(expected, desired)` 的语义是**原子地**做这件事：
//      若 head_ 当前 == expected：把 head_ 改成 desired，返回 true；
//      否则（说明别的线程刚改过 head_）：**把 expected 更新成 head_ 的最新值**，返回 false。
//  注意那个"失败时回写 expected"的细节——它让重试循环天然好写：失败了 expected 已经是
//  最新值，直接据此重算 desired 再试即可。`_weak` 版允许"伪失败"（偶尔无理由返回 false），
//  但在循环里用更快，所以无锁循环都用 `_weak`。
//
//  【push 怎么做到无锁？——"乐观重试"】
//      新建节点 n；
//      do {
//          n->next = 当前 head;          // 乐观假设：head 不会变
//          // 试图把 head 从"我看到的旧值"换成 n
//      } while (CAS(head, 旧值, n) 失败); // 失败说明 head 被人改了，CAS 已回写最新值，重来
//  没有锁，只有"读当前 head → 试着原子替换 → 被抢就重试"。
//
//  【pop 怎么做？】
//      读 old = head；
//      while (old != null 且 CAS(head, old, old->next) 失败) { }  // 失败时 old 已被回写为最新 head
//      若 old == null：栈空，返回 false；
//      否则取出 old->value，回收 old，返回 true。
//
//  【内存序为什么用 acquire/release（回顾 B3）】
//  push 成功要让"我刚写好的节点内容"对将来 pop 它的线程可见 → 用 **release**。
//  pop 读取/摘下节点要能看到 push 当时写入的内容 → 用 **acquire**。一句话：
//  release 发布、acquire 接收，配对起来形成 happens-before，别的线程才看得到节点数据。
//
//  【⚠️ ABA 问题（本模块最重要的考点）】
//  pop 用 CAS 判断"head 还等于我读到的 old 吗？相等就认为栈没变"。但"相等"≠"没变过"！
//  设栈是 A→B→C，head==A：
//    线程1 读 old=A，算出 old->next=B，正打算 CAS(head, A, B)；此时被切走。
//    线程2 把 A pop 掉（head=B），又把 B pop 掉（head=C），**又 push 回一个新节点，地址恰好还是 A**
//        （内存分配器复用了刚释放的地址），并让 A->next=C，于是 head 又==A。
//    线程1 醒来，CAS(head, A, B) **看到 head==A 就成功了**！可此刻 B 早已被弹出/释放，
//        head 被错误地设成了一个**悬空指针 B** → 崩溃 / 数据损坏。这就是 **ABA**：
//        值从 A 变到 B…又变回 A，CAS 无法察觉中间发生过变化。
//  常见解法（了解即可，工程里据场景选）：
//    · **带标签指针 / 版本号**：把 head 做成 (指针, 计数) 对，每次改动计数 +1，
//      CAS 连计数一起比，A 回来时计数已不同 → CAS 失败（DCAS / tagged pointer）。
//    · **危险指针 (hazard pointer)** / **RCU** / **epoch**：延迟回收节点，确保没人还在读它才 free，
//      从根上消除"地址被复用"。
//    · 用支持安全回收的库（如 folly、boost.lockfree）。
//
//  【⚠️ 本模块的简化边界（务必读）】
//  pop 里 `delete old` 在**并发 pop**时是不安全的（另一线程可能正读 old->next → use-after-free，
//  这正是 ABA/回收难题的来源，需危险指针才能根治）。为聚焦"CAS 重试"这一核心、不引入危险指针的
//  复杂度，本模块的并发测试只做 **多线程并发 push + 之后单线程 drain（逐个 pop）**：push 永不 free、
//  drain 时无并发，故安全。理解了这一点，你就理解了"为什么无锁栈看着简单、工业级实现却很难"。
//
// =============================================================================
#pragma once

#include <atomic>   // std::atomic
#include <cstddef>  // std::size_t
#include <utility>  // std::move

namespace cppbc {

// ---------------------------------------------------------------------------
//  无锁栈：head_ 是原子指针，push/pop 用 CAS 重试，不使用任何 mutex。
// ---------------------------------------------------------------------------
template <class T>
class LockFreeStack {
    struct Node {
        T     value;
        Node* next;
        explicit Node(T v) : value(std::move(v)), next(nullptr) {}
    };

    std::atomic<Node*>       head_{nullptr};  // 栈顶（唯一的并发同步点）
    std::atomic<std::size_t> size_{0};        // 元素个数（教学/测试用，relaxed 维护）

public:
    LockFreeStack() = default;

    // 析构：单线程销毁，沿链表把节点逐个释放（此处已给好，无需修改）。
    ~LockFreeStack() {
        Node* p = head_.load(std::memory_order_relaxed);
        while (p) {
            Node* nxt = p->next;
            delete p;
            p = nxt;
        }
    }

    // 无锁栈不可拷贝（拷贝并发容器没有良好定义的语义）。
    LockFreeStack(const LockFreeStack&)            = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    // ===================== TODO(B4-1) push ==========================
    //  无锁入栈：新建节点，用 CAS 把它接到栈顶；被别的线程抢了就重试。
    //    Node* n = new Node(std::move(value));
    //    n->next = head_.load(std::memory_order_relaxed);
    //    while (!head_.compare_exchange_weak(
    //               n->next, n,
    //               std::memory_order_release,   // 成功：发布"我写好的节点内容"
    //               std::memory_order_relaxed))  // 失败：仅需把 n->next 回写为最新 head
    //    { /* 失败时 compare_exchange 已把最新 head 写进 n->next，循环体留空即可 */ }
    //    size_.fetch_add(1, std::memory_order_relaxed);
    // ===============================================================
    void push(T value) {
        Node* n = new Node(std::move(value));
        n->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(
            n->next, n,
            std::memory_order_release,
            std::memory_order_relaxed)) {
        }
        size_.fetch_add(1, std::memory_order_relaxed);  
    }

    // ===================== TODO(B4-2) pop ===========================
    //  无锁出栈：用 CAS 把栈顶从 old 换成 old->next；成功则取值、回收节点。
    //    Node* old = head_.load(std::memory_order_acquire);
    //    while (old && !head_.compare_exchange_weak(
    //                      old, old->next,
    //                      std::memory_order_acquire,    // 成功：要能看到 push 写入的节点内容
    //                      std::memory_order_relaxed))   // 失败：把 old 回写为最新 head 后重试
    //    { /* 循环体留空：失败时 old 已被更新 */ }
    //    if (!old) return false;                          // 栈空
    //    out = std::move(old->value);
    //    delete old;                                      // 单线程 drain 场景下安全（见导读）
    //    size_.fetch_sub(1, std::memory_order_relaxed);
    //    return true;
    // ===============================================================
    bool pop(T& out) {
        // TODO
        Node* old = head_.load(std::memory_order_acquire);
        while (old && !head_.compare_exchange_weak(
            old, old->next,
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
        }
        if (!old) return false;
        out = std::move(old->value);
        delete old;
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    // 是否为空（以原子读 head_ 为准）。
    bool empty() const {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    // 当前元素个数（近似值；并发中仅供观测/测试）。
    std::size_t size() const {
        return size_.load(std::memory_order_relaxed);
    }
};

} // namespace cppbc
