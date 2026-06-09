// =============================================================================
//  D1 · 手写 LRU 缓存（哈希表 + 双向链表，O(1) get/put）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个 **LRU（Least Recently Used，最近最少使用）缓存**：容量固定，装满后
//  再插入新元素时，淘汰"最久没被用过"的那个。它是缓存淘汰策略里最经典的一种，
//  Redis、CPU cache、各种本地缓存都用得到，也是大厂面试的"必背手写题"。
//
//  【核心难点：怎么让 get 和 put 都做到 O(1)？】
//  我们要同时满足两件事：
//    (A) 按 key 快速查找 value          → 需要「哈希表」
//    (B) 维护元素的"使用先后顺序"，并能 O(1) 地把某个元素提到最前、从最后淘汰
//                                        → 需要「双向链表」
//  单用任何一个都不够：哈希表查得快但记不住顺序；链表记得住顺序但查找是 O(n)。
//  把两者**组合**起来，各取所长，才有 O(1) 的 get/put。这正是本题的精髓。
//
//  【数据结构组合】
//      items_:  双向链表，每个结点存 (key, value)。
//               约定【表头 = 最近用过(MRU)】，【表尾 = 最久没用(LRU)】。
//               front --------------------------------------> back
//               (最新)  (k,v) <-> (k,v) <-> (k,v) <-> (k,v)  (最旧，将被淘汰)
//
//      index_:  哈希表  key -> "指向该 key 在 items_ 里那个结点的链表迭代器"
//               有了它，给定 key 就能 O(1) 找到链表结点，不必遍历链表。
//
//  【为什么用 std::list + 存「迭代器」？关键魔法：splice 不会让迭代器失效】
//  std::list 的迭代器/指针在 insert、erase（除被删的那个）、尤其 **splice** 之后
//  依然有效。我们用 list::splice 把一个结点 O(1) 地"挪到表头"，而 index_ 里存的
//  那个迭代器**仍然指向同一个结点、依旧可用**——这就是哈希表存迭代器的底气。
//  （换成 vector 就不行：vector 一插入/删除，迭代器和指针全失效。）
//
//  【两个操作的语义】
//   - get(key)：命中 → 把该结点提到表头（标记为"刚用过"），返回 value 指针；
//                未命中 → 返回 nullptr。
//   - put(key,value)：
//        已存在 → 更新 value，并提到表头；
//        不存在 → 在表头插入新结点；若超出容量，淘汰表尾（LRU）那个。
//
// =============================================================================
#pragma once

#include <cstddef>        // std::size_t
#include <list>           // std::list（双向链表）
#include <unordered_map>  // std::unordered_map（哈希索引）
#include <utility>        // std::pair

namespace cppbc {

template <class K, class V>
class LRUCache {
public:
    // 链表结点类型 = (key, value)；以及"指向结点的迭代器"类型别名。
    using Node   = std::pair<K, V>;
    using ListIt = typename std::list<Node>::iterator;

    // 固定容量。capacity 个元素，满了再插就淘汰最久未用的。
    explicit LRUCache(std::size_t capacity) : capacity_(capacity) {}

    // ===================== TODO(D1-1) get ============================
    // 命中：把结点提到表头（最近使用），返回指向其 value 的指针。
    // 未命中：返回 nullptr。
    //   auto it = index_.find(key);
    //   if (it == index_.end()) return nullptr;          // 没这个 key
    //   // it->second 是"指向链表结点的迭代器"。把该结点挪到表头（O(1)，迭代器不失效）：
    //   items_.splice(items_.begin(), items_, it->second);
    //   return &it->second->second;                      // 结点的 .second 就是 value
    // =================================================================
    V* get(const K& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return nullptr;
        items_.splice(items_.begin(), items_, it->second);
        return &it->second->second;
    }

    // ===================== TODO(D1-2) put ============================
    // 已存在则更新并提到表头；不存在则在表头插入，必要时淘汰表尾。
    //   auto it = index_.find(key);
    //   if (it != index_.end()) {                        // ── 已存在：更新 + 提到表头
    //       it->second->second = value;
    //       items_.splice(items_.begin(), items_, it->second);
    //       return;
    //   }
    //   items_.emplace_front(key, value);                // ── 不存在：表头插入新结点
    //   index_[key] = items_.begin();                    //    记下它的迭代器
    //   if (items_.size() > capacity_) evict_lru_();     //    超容 → 淘汰表尾
    // =================================================================
    void put(const K& key, const V& value) {
        auto it = index_.find(key);
        if (it != index_.end()) {
            it->second->second = value;
            items_.splice(items_.begin(), items_, it->second);
            return;
        }
        items_.emplace_front(key, value);
        index_[key] = items_.begin();
        if (items_.size() > capacity_) evict_lru_();
    }

    // ---- 下面是已给出的部分，无需修改 ----

    // 只读地查询 key 是否在缓存里（**不**改变使用顺序，方便测试观察淘汰结果）。
    bool contains(const K& key) const {
        return index_.find(key) != index_.end();
    }

    std::size_t size()     const noexcept { return items_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    bool        empty()    const noexcept { return items_.empty(); }

private:
    // ===================== TODO(D1-3) evict_lru_ =====================
    // 淘汰"最久未使用"的元素：它就在链表表尾。
    // 注意顺序：要在结点还活着时，先用它的 key 把哈希索引项删掉，再 pop_back。
    //   index_.erase(items_.back().first);   // 先从哈希表移除该 key
    //   items_.pop_back();                   // 再从链表删掉表尾结点
    // =================================================================
    void evict_lru_() {
        index_.erase(items_.back().first);
        items_.pop_back();
    }

    std::size_t                       capacity_;   // 容量上限
    std::list<Node>                   items_;      // 表头=MRU，表尾=LRU
    std::unordered_map<K, ListIt>     index_;      // key -> 链表结点迭代器
};

} // namespace cppbc
