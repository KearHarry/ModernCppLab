// =============================================================================
//  C6 · 跳表 SkipList（有序映射 int→int）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个**有序**的 key→value 容器，支持 O(log n) 期望复杂度的查找/插入/删除，并能按
//  key 升序遍历——但它**不用平衡树**（红黑树/AVL），而用一种更简单、更易写对的随机化结构：
//  **跳表**。Redis 的有序集合(zset)、LevelDB/RocksDB 的 memtable 底层都是跳表。面试经典题：
//  "不用树，怎么实现一个有序 map？"
//
//  【核心直觉：给有序链表加"快速通道"】
//  一条排好序的单链表，查找要逐个走 O(n)。跳表在它上面叠加几层"快速通道"：
//      L3: head ----------------------------> 50 --------> nil
//      L2: head -------> 20 ----------------> 50 --------> nil
//      L1: head -> 10 -> 20 -> 30 ----------> 50 -> 70 --> nil
//      L0: head -> 10 -> 20 -> 30 -> 40 -> 45 -> 50 -> 70 -> nil   ← 最底层含全部元素
//  查 45：从最高层往右走，走过头就**下降一层**继续。高层一步跨过很多节点，于是平均
//  只看 O(log n) 个节点。每个节点的"高度"在插入时**抛硬币随机**决定（约一半概率多升一层），
//  统计上自然形成上面这种稀疏的塔状分布——**无需旋转、无需再平衡**，这是跳表比平衡树好写的关键。
//
//  【节点长什么样】
//    struct Node { key; value; vector<Node*> forward; };   // forward[i] = 本节点在第 i 层的后继
//  节点的 forward 数组长度 = 它的随机高度。head_ 是哨兵，高度拉满到 kMaxLevel，不存数据。
//
//  【三个操作的统一套路：从顶层往下"逼近"】
//    Node* x = head_;
//    for (int i = level_-1; i >= 0; --i)                       // 从当前最高层往下
//        while (x->forward[i] && x->forward[i]->key < key)     // 同层能往右就往右（只要不越过 key）
//            x = x->forward[i];
//    // 出来后 x 是"最后一个 key 小于目标的节点"，x->forward[0] 就是候选
//  · find：看 x->forward[0] 是不是目标。
//  · insert/erase：下降时把每层"即将下降处的 x"记到 update[i]，之后在这些位置改指针。
//    update[i] 就是"第 i 层里、新节点该插在谁后面 / 待删节点在第 i 层的前驱"。
//
//  【和平衡树(map)的对照（面试考点）】
//    · 复杂度：都是 O(log n)，跳表是**期望**（随机），树是**最坏**保证。
//    · 实现难度：跳表插入/删除只改指针、无旋转，**好写好懂**；红黑树旋转+变色极易写错。
//    · 并发：跳表更容易做成**无锁/细粒度锁**（改动局部化），故 Redis/LevelDB 青睐它。
//    · 内存：跳表每节点多个指针（平均 ~2 个），树每节点 2 孩子 +1 父 + 颜色。
//
//  【你会实现的 3 个 TODO】（random_level、析构、size/empty/contains/for_each 已给好）
//    C6-1  find   —— 顶层往下逼近，返回 &value 或 nullptr（核心遍历套路）
//    C6-2  insert —— 记录 update[]，已存在则改值，否则按随机高度新建并在各层接入
//    C6-3  erase  —— 记录 update[]，存在则在它出现的每层断开、删除、必要时降 level_
//
//  ⚠ 随机性只影响**结构高度/性能**，不影响**正确性**：无论硬币怎么抛，find/insert/erase 的
//     逻辑结果都必须正确、遍历都必须有序。测试只验证这些确定性的逻辑结果。
//
// =============================================================================
#pragma once

#include <cstddef>  // std::size_t
#include <random>   // std::mt19937, std::uniform_int_distribution
#include <vector>   // std::vector

namespace cppbc {

class SkipList {
    static constexpr int kMaxLevel = 16;  // 最高层数（支持约 2^16 个元素）

    struct Node {
        int               key;
        int               value;
        std::vector<Node*> forward;  // forward[i] = 第 i 层的后继；大小 = 本节点高度
        Node(int k, int v, int height) : key(k), value(v), forward(height, nullptr) {}
    };

    Node*       head_;       // 哨兵节点：高度 kMaxLevel，key/value 不用
    int         level_ = 1;  // 当前实际用到的最高层数（1..kMaxLevel）
    std::size_t size_  = 0;  // 元素个数
    std::mt19937 rng_;       // 随机层高用的随机数引擎（固定种子→可复现）

    // 抛硬币决定新节点高度：每多一层约 1/2 概率，封顶 kMaxLevel。已给好。
    int random_level() {
        int lvl = 1;
        std::uniform_int_distribution<int> coin(0, 1);
        while (lvl < kMaxLevel && coin(rng_) == 1) ++lvl;
        return lvl;
    }

public:
    SkipList()
        : head_(new Node(0, 0, kMaxLevel)), rng_(0xC6C6C6C6u) {}

    ~SkipList() {  // 沿最底层把所有节点（含哨兵）逐个释放。已给好。
        Node* n = head_;
        while (n) {
            Node* nx = n->forward[0];
            delete n;
            n = nx;
        }
    }

    SkipList(const SkipList&)            = delete;
    SkipList& operator=(const SkipList&) = delete;

    // ===================== TODO(C6-1) find ==========================
    //  查找 key：从顶层往下逼近，命中返回指向其 value 的指针，否则 nullptr。
    //    Node* x = head_;
    //    for (int i = level_ - 1; i >= 0; --i)
    //        while (x->forward[i] && x->forward[i]->key < key)
    //            x = x->forward[i];
    //    x = x->forward[0];                       // 候选：最后一个 <key 节点的下一个
    //    if (x && x->key == key) return &x->value;
    //    return nullptr;
    // ===============================================================
    const int* find(int key) const {
        Node* x = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
        }
        x = x->forward[0];
        if (x && x->key == key) return &x->value;
        return nullptr;
    }

    // ===================== TODO(C6-2) insert ========================
    //  插入或更新 key→value。
    //    std::vector<Node*> update(kMaxLevel, head_);  // 各层"前驱"，高层默认是 head_
    //    Node* x = head_;
    //    for (int i = level_ - 1; i >= 0; --i) {
    //        while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
    //        update[i] = x;                            // 第 i 层将在 x 之后插入/接续
    //    }
    //    Node* c = update[0]->forward[0];
    //    if (c && c->key == key) { c->value = value; return; }  // 已存在 → 改值即可
    //    int lvl = random_level();                     // 新节点的随机高度
    //    if (lvl > level_) level_ = lvl;               // 拔高了当前层数（新层前驱已是 head_）
    //    Node* n = new Node(key, value, lvl);
    //    for (int i = 0; i < lvl; ++i) {               // 在每一层把 n 接进去
    //        n->forward[i]          = update[i]->forward[i];
    //        update[i]->forward[i]  = n;
    //    }
    //    ++size_;
    // ===============================================================
    void insert(int key, int value) {
        std::vector<Node*> update(kMaxLevel, head_);
        Node* x = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
            update[i] = x;
        }
        Node* c = update[0]->forward[0];
        if (c && c->key == key) { c->value = value; return; }
        int lvl = random_level();
        if (lvl > level_) level_ = lvl;
        Node* n = new Node(key, value, lvl);
        for (int i = 0; i < lvl; ++i) {
            n->forward[i]         = update[i]->forward[i];
            update[i]->forward[i] = n;
        }
        ++size_;
    }

    // ===================== TODO(C6-3) erase =========================
    //  删除 key；成功返回 true，不存在返回 false。
    //    std::vector<Node*> update(kMaxLevel, head_);
    //    Node* x = head_;
    //    for (int i = level_ - 1; i >= 0; --i) {
    //        while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
    //        update[i] = x;
    //    }
    //    Node* c = update[0]->forward[0];
    //    if (!c || c->key != key) return false;        // 不存在
    //    for (int i = 0; i < level_; ++i) {            // 在 c 出现的每一层把它摘掉
    //        if (update[i]->forward[i] != c) break;    // 更高层已不含 c → 后面也不含
    //        update[i]->forward[i] = c->forward[i];
    //    }
    //    delete c;
    //    while (level_ > 1 && head_->forward[level_ - 1] == nullptr) --level_;  // 收缩空的高层
    //    --size_;
    //    return true;
    // ===============================================================
    bool erase(int key) {
        std::vector<Node*> update(kMaxLevel, head_);
        Node* x = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (x->forward[i] && x->forward[i]->key < key) x = x->forward[i];
            update[i] = x;
        }
        Node* c = update[0]->forward[0];
        if (!c || c->key != key) return false;
        for (int i = 0; i < level_; ++i) {
            if (update[i]->forward[i] != c) break;
            update[i]->forward[i] = c->forward[i];
        }
        delete c;
        while (level_ > 1 && head_->forward[level_ - 1] == nullptr) --level_;
        --size_;
        return true;
    }

    bool contains(int key) const { return find(key) != nullptr; }  // 已给好
    std::size_t size()  const noexcept { return size_; }
    bool        empty() const noexcept { return size_ == 0; }

    // 按 key 升序遍历，对每个元素调用 f(key, value)。已给好（走最底层）。
    template <class F>
    void for_each(F f) const {
        for (Node* n = head_->forward[0]; n; n = n->forward[0]) f(n->key, n->value);
    }
};

} // namespace cppbc
