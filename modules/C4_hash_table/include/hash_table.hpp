// =============================================================================
//  C4 · 手写 unordered_map（哈希表 · 链地址法 + 负载因子 + rehash）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个简化版 std::unordered_map：用「哈希表」做 O(1) 平均时间的增删查。
//  我们采用最经典、最好懂的 **链地址法（separate chaining）**：
//
//      buckets_:  [0] -> (k,v) -> (k,v)            ← 每个"桶"是一条链表
//                 [1] -> (k,v)
//                 [2] ->                            ← 空桶
//                 [3] -> (k,v) -> (k,v) -> (k,v)
//                  ...
//
//  查 key 的过程只有两步：
//    1) 算出它该落在哪个桶： idx = hash(key) % bucket_count
//    2) 在这条桶链表里线性扫描，比对 key 是否相等。
//  只要哈希分得均匀、每条链都很短，第 2 步几乎是常数 → 整体平均 O(1)。
//
//  【为什么需要 rehash（再散列 / 扩容）？】
//  桶的数量是固定的，元素越塞越多，每条链就越长，查找退化成 O(n)。
//  我们用 **负载因子 load_factor = size / bucket_count** 衡量"挤不挤"：
//      - 它 ≈ 平均每条桶链的长度；
//      - 一旦超过阈值 max_load_factor（这里取 1.0），就把桶数翻倍（rehash），
//        并把所有元素按"新桶数"重新分配一遍 → 链重新变短，查询重回 O(1)。
//
//  【rehash 最容易踩的坑：桶数变了，下标必须重算！】
//  桶下标 idx = hash(key) % bucket_count 依赖 bucket_count。
//  一旦 rehash 把桶数从 8 变成 16，同一个 key 的 idx 通常也变了。
//  所以：**先判断要不要 rehash，rehash 之后再重新算 idx**，
//  千万不能用扩容前算好的旧 idx 去插新桶（经典 bug）。
//
//  【对象布局】
//      std::vector<std::list<std::pair<K,V>>> buckets_;  // 桶数组，每桶一条链表
//      std::size_t size_;            // 当前元素个数
//      float       max_load_factor_; // 负载因子阈值（默认 1.0）
//      Hash        hash_;            // 哈希函数对象（默认 std::hash<K>）
//
// =============================================================================
#pragma once

#include <cstddef>     // std::size_t
#include <functional>  // std::hash
#include <list>        // std::list（桶链表）
#include <utility>     // std::pair, std::move
#include <vector>      // std::vector（桶数组）

namespace cppbc {

template <class K, class V, class Hash = std::hash<K>>
class HashMap {
public:
    // 初始 8 个桶。桶数组里每个元素都是一条（初始为空的）链表。
    HashMap() : buckets_(8) {}

    // ===================== TODO(C4-1) find ===========================
    // 查找 key：找到则返回指向其 value 的指针，找不到返回 nullptr。
    //   提示（和下面 GIVEN 的 contains 几乎一样，只是返回指针而非 bool）：
    //     auto& chain = buckets_[bucket_index_(key)];   // 定位到该 key 所在的桶
    //     for (auto& kv : chain)                        // 在桶链里线性扫描
    //         if (kv.first == key) return &kv.second;   // 命中 → 返回 value 地址
    //     return nullptr;                               // 整条链都没有 → 没找到
    // =================================================================
    V* find(const K& key) {
        // TODO
        (void)key;
        return nullptr;
    }

    // ===================== TODO(C4-2) operator[] =====================
    // 取 key 对应的 value 引用；不存在则**插入**一个默认值 V{} 再返回其引用
    //（这正是 m["x"] = 1 能工作的原因：先插入再赋值）。
    //   1) 已存在就直接返回：
    //        if (V* p = find(key)) return *p;
    //   2) 不存在 → 先判断"插入这一个之后"会不会超过负载因子阈值，超了就扩容：
    //        if (static_cast<float>(size_ + 1) / buckets_.size() > max_load_factor_)
    //            rehash(buckets_.size() * 2);
    //   3) 【关键】rehash 可能改变了桶数，必须**重新**计算桶下标：
    //        std::size_t idx = bucket_index_(key);
    //   4) 在该桶尾部插入 (key, V{})，更新计数，返回新元素的 value 引用：
    //        buckets_[idx].emplace_back(key, V{});
    //        ++size_;
    //        return buckets_[idx].back().second;
    // =================================================================
    V& operator[](const K& key) {
        // TODO：骨架先返回一个共享的"假"引用，保证不崩溃（size_ 仍为 0 → 测试会先红）
        (void)key;
        static V dummy{};
        return dummy;
    }

    // ===================== TODO(C4-3) erase ==========================
    // 删除 key：删掉了返回 true，本来就不存在返回 false。
    //   auto& chain = buckets_[bucket_index_(key)];
    //   for (auto it = chain.begin(); it != chain.end(); ++it) {
    //       if (it->first == key) {
    //           chain.erase(it);   // 从链表里摘除该结点
    //           --size_;
    //           return true;
    //       }
    //   }
    //   return false;
    // =================================================================
    bool erase(const K& key) {
        // TODO
        (void)key;
        return false;
    }

    // ===================== TODO(C4-4) rehash =========================
    // 把桶数扩到 new_bucket_count，并把所有元素按新桶数重新分配。
    //   std::vector<std::list<std::pair<K,V>>> new_buckets(new_bucket_count);
    //   for (auto& chain : buckets_)                 // 遍历每个旧桶
    //       for (auto& kv : chain) {                 // 遍历桶里每个元素
    //           std::size_t idx = hash_(kv.first) % new_bucket_count;  // 按"新桶数"重算
    //           new_buckets[idx].push_back(std::move(kv));             // 搬过去（移动，省拷贝）
    //       }
    //   buckets_ = std::move(new_buckets);           // 用新桶数组替换旧的
    // =================================================================
    void rehash(std::size_t new_bucket_count) {
        // TODO
        (void)new_bucket_count;
    }

    // ---- 下面是已给出的部分，无需修改 ----

    // 是否包含 key（只读）。教学用：内部就是"定位桶 → 扫链"这套标准动作，
    // 你写 find 时照着改成"返回 value 指针"即可。
    bool contains(const K& key) const {
        const auto& chain = buckets_[bucket_index_(key)];
        for (const auto& kv : chain)
            if (kv.first == key) return true;
        return false;
    }

    // 负载因子 = 元素数 / 桶数 ≈ 平均每条桶链的长度。
    float load_factor() const noexcept {
        return buckets_.empty() ? 0.0f
                                : static_cast<float>(size_) / buckets_.size();
    }

    std::size_t bucket_count()     const noexcept { return buckets_.size(); }
    float       max_load_factor()  const noexcept { return max_load_factor_; }
    std::size_t size()             const noexcept { return size_; }
    bool        empty()            const noexcept { return size_ == 0; }

private:
    // 给定的私有帮手：算出 key 应落在哪个桶。注意它依赖当前桶数 buckets_.size()。
    std::size_t bucket_index_(const K& key) const {
        return hash_(key) % buckets_.size();
    }

    std::vector<std::list<std::pair<K, V>>> buckets_;          // 桶数组
    std::size_t size_            = 0;                          // 元素个数
    float       max_load_factor_ = 1.0f;                      // 负载因子阈值
    Hash        hash_{};                                       // 哈希函数对象
};

} // namespace cppbc
