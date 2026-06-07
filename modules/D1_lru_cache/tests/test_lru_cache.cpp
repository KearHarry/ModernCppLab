// =============================================================================
//  D1 测试文件 —— 检查你的 lru_cache.hpp 写对了没有
// -----------------------------------------------------------------------------
//  设计说明：凡要按指针读 value 的用例，先 ASSERT_TRUE(p != nullptr) 挡住；
//  凡要观察淘汰结果的用例，先用 ASSERT_EQ 校验 size()。骨架阶段
//  （get 恒为 nullptr、put 不真正插入、size 恒为 0）会先中止，绝不解引用空指针，
//  所以一开始只会变红、不会崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "lru_cache.hpp"

#include <cstddef>
#include <string>

using namespace cppbc;

// ---- 基本 put / get：命中返回 value 指针，未命中返回 nullptr ----
TEST(D1_basic, put_then_get) {
    LRUCache<int, std::string> c(2);
    c.put(1, "one");
    c.put(2, "two");

    ASSERT_EQ(c.size(), std::size_t{2});             // 骨架阶段在此中止
    std::string* p = c.get(1);
    ASSERT_TRUE(p != nullptr);
    EXPECT_EQ(*p, std::string("one"));
    EXPECT_TRUE(c.get(99) == nullptr);               // 未命中
}

// ---- put 同一个 key 是"更新"不是"新增"：size 不变，值被改 ----
TEST(D1_update, put_updates_value) {
    LRUCache<int, int> c(2);
    c.put(1, 10);
    c.put(1, 20);                                    // 同 key，更新

    ASSERT_EQ(c.size(), std::size_t{1});             // 仍是 1
    int* p = c.get(1);
    ASSERT_TRUE(p != nullptr);
    EXPECT_EQ(*p, 20);
}

// ---- 超出容量淘汰"最久未使用"的元素（表尾） ----
TEST(D1_evict, evicts_least_recently_used) {
    LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(3, 3);                                     // 超容 → 淘汰最旧的 key=1

    ASSERT_EQ(c.size(), std::size_t{2});
    EXPECT_FALSE(c.contains(1));                     // 被淘汰
    EXPECT_TRUE(c.contains(2));
    EXPECT_TRUE(c.contains(3));
}

// ---- get 会"刷新"使用顺序：被 get 过的 key 逃过淘汰 ----
TEST(D1_promote, get_promotes_to_mru) {
    LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);

    int* p = c.get(1);                               // 用了一下 1 → 1 变成最新，2 变最旧
    ASSERT_TRUE(p != nullptr);

    c.put(3, 3);                                     // 超容 → 淘汰最旧的 key=2（不是 1！）
    ASSERT_EQ(c.size(), std::size_t{2});
    EXPECT_TRUE(c.contains(1));                      // 因为 get 过，活下来了
    EXPECT_FALSE(c.contains(2));                     // 被淘汰
    EXPECT_TRUE(c.contains(3));
}

// ---- put 已存在的 key 也会"刷新"使用顺序 ----
TEST(D1_promote, put_existing_promotes) {
    LRUCache<int, int> c(2);
    c.put(1, 1);
    c.put(2, 2);
    c.put(1, 100);                                   // 更新 1 → 1 变最新，2 变最旧

    c.put(3, 3);                                     // 超容 → 淘汰最旧的 key=2
    ASSERT_EQ(c.size(), std::size_t{2});
    EXPECT_TRUE(c.contains(1));
    EXPECT_FALSE(c.contains(2));
    EXPECT_TRUE(c.contains(3));

    int* p = c.get(1);
    ASSERT_TRUE(p != nullptr);
    EXPECT_EQ(*p, 100);                              // 值确实更新了
}
