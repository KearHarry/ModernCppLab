// =============================================================================
//  C4 测试文件 —— 检查你的 hash_table.hpp 写对了没有
// -----------------------------------------------------------------------------
//  设计说明：凡是要按指针读取 value 的用例，都先 ASSERT_TRUE(p != nullptr) 把它挡住；
//  凡是要按下标读 value 的用例，先用 ASSERT_EQ 校验 size()。这样骨架阶段
//  （find 恒为 nullptr、operator[] 不真正插入、size 恒为 0）会先中止，绝不解引用空指针，
//  所以一开始只会变红、不会崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "hash_table.hpp"

#include <cstddef>
#include <string>

using namespace cppbc;

// ---- 基本插入与查找：find 命中返回 value 指针，未命中返回 nullptr ----
TEST(C4_basic, insert_and_find) {
    HashMap<std::string, int> m;
    m["alice"] = 1;
    m["bob"]   = 2;
    m["carol"] = 3;

    ASSERT_EQ(m.size(), std::size_t{3});             // 骨架阶段在此中止，避免空指针解引用
    int* p = m.find("bob");
    ASSERT_TRUE(p != nullptr);                        // 命中才允许解引用
    EXPECT_EQ(*p, 2);

    EXPECT_TRUE(m.contains("alice"));
    EXPECT_FALSE(m.contains("zzz"));
    EXPECT_TRUE(m.find("zzz") == nullptr);            // 未命中返回 nullptr
}

// ---- operator[] 对已存在的 key 是"改"不是"增"：size 不变 ----
TEST(C4_index, operator_index_updates) {
    HashMap<std::string, int> m;
    m["k"] = 10;
    ASSERT_EQ(m.size(), std::size_t{1});

    m["k"] = 99;                                      // 覆盖同一个 key
    EXPECT_EQ(m.size(), std::size_t{1});              // 还是 1，没有新增

    int* p = m.find("k");
    ASSERT_TRUE(p != nullptr);
    EXPECT_EQ(*p, 99);                                // 值被更新
}

// ---- operator[] 访问不存在的 key 会插入默认值 V{}（int → 0） ----
TEST(C4_index, default_inserts_zero) {
    HashMap<std::string, int> m;
    int v = m["new"];                                 // 触发"先插入默认值再返回引用"
    EXPECT_EQ(v, 0);

    ASSERT_EQ(m.size(), std::size_t{1});              // 确实插入了一个元素
    EXPECT_TRUE(m.contains("new"));
}

// ---- erase 删除存在的 key 返回 true 并减小 size；删不存在的返回 false ----
TEST(C4_erase, erase_removes) {
    HashMap<std::string, int> m;
    m["a"] = 1;
    m["b"] = 2;

    ASSERT_EQ(m.size(), std::size_t{2});
    EXPECT_TRUE(m.erase("a"));
    EXPECT_EQ(m.size(), std::size_t{1});
    EXPECT_FALSE(m.contains("a"));
    EXPECT_TRUE(m.contains("b"));
    EXPECT_FALSE(m.erase("a"));                       // 已经没有了，再删返回 false
}

// ---- 大量插入触发自动 rehash：桶数增长、负载因子受控、所有数据仍可正确查回 ----
TEST(C4_rehash, many_keys_and_rehash) {
    HashMap<int, int> m;
    const int N = 1000;
    for (int i = 0; i < N; ++i) m[i] = i * i;

    ASSERT_EQ(m.size(), std::size_t(N));              // 骨架阶段在此中止
    EXPECT_GT(m.bucket_count(), std::size_t{8});      // 从 8 桶扩容上来了
    EXPECT_LE(m.load_factor(), m.max_load_factor());  // 负载因子始终不超过阈值

    bool all_ok = true;
    for (int i = 0; i < N; ++i) {
        int* p = m.find(i);
        if (!p || *p != i * i) { all_ok = false; break; }   // 短路：p 为空就不解引用
    }
    EXPECT_TRUE(all_ok);                              // 每个 key 都查回了正确的 value
}
