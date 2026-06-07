// =============================================================================
//  C6 测试文件 —— 检查你的跳表写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：find 恒 nullptr、insert 为空操作、erase 恒 false。跳表始终为空，取值前都
//  用 ASSERT_* 挡住，所以一开始只变红、不崩溃。random_level/析构/size/contains/
//  for_each 已给好。随机层高只影响结构，不影响这些确定性的逻辑断言。
// =============================================================================
#include "test_framework.hpp"
#include "skip_list.hpp"

#include <vector>

using namespace cppbc;

// 收集 (key,value)，顺序即遍历顺序（应升序）。
static std::vector<std::pair<int,int>> dump(const SkipList& sl) {
    std::vector<std::pair<int,int>> out;
    sl.for_each([&](int k, int v) { out.emplace_back(k, v); });
    return out;
}

// ---- 插入后能查到，缺失返回 nullptr ----
TEST(C6_find, insert_then_find) {
    SkipList sl;
    EXPECT_TRUE(sl.empty());
    sl.insert(10, 100);
    sl.insert(20, 200);
    sl.insert(5,  50);
    EXPECT_EQ(sl.size(), static_cast<std::size_t>(3));

    const int* p = sl.find(20);
    ASSERT_NE(p, nullptr);                           // 骨架 find 恒 nullptr → 在此中止
    EXPECT_EQ(*p, 200);

    const int* q = sl.find(5);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(*q, 50);

    EXPECT_EQ(sl.find(999), nullptr);                // 不存在 → nullptr
    EXPECT_TRUE(sl.contains(10));
    EXPECT_FALSE(sl.contains(11));
}

// ---- 乱序插入，遍历得到升序 ----
TEST(C6_order, iterates_in_sorted_order) {
    SkipList sl;
    int keys[] = {42, 7, 19, 4, 100, 23, 1};
    for (int k : keys) sl.insert(k, k * 10);

    std::vector<std::pair<int,int>> got = dump(sl);
    ASSERT_EQ(got.size(), static_cast<std::size_t>(7)); // 骨架为空 → 在此中止
    int expected_sorted[] = {1, 4, 7, 19, 23, 42, 100};
    for (std::size_t i = 0; i < got.size(); ++i) {
        EXPECT_EQ(got[i].first,  expected_sorted[i]);
        EXPECT_EQ(got[i].second, expected_sorted[i] * 10);
    }
}

// ---- 重复 key 是更新而非新增，size 不变 ----
TEST(C6_update, duplicate_key_updates_value) {
    SkipList sl;
    sl.insert(7, 70);
    sl.insert(7, 777);                               // 同 key 再插 → 改值
    EXPECT_EQ(sl.size(), static_cast<std::size_t>(1));

    const int* p = sl.find(7);
    ASSERT_NE(p, nullptr);                           // 骨架 → 在此中止
    EXPECT_EQ(*p, 777);
}

// ---- 删除：删掉后查不到，其余不受影响，size 递减；删不存在返回 false ----
TEST(C6_erase, erase_removes_only_target) {
    SkipList sl;
    for (int k = 1; k <= 5; ++k) sl.insert(k, k);
    ASSERT_EQ(sl.size(), static_cast<std::size_t>(5)); // 骨架为空 → 在此中止

    EXPECT_TRUE(sl.erase(3));                         // 删中间
    EXPECT_FALSE(sl.contains(3));
    EXPECT_EQ(sl.size(), static_cast<std::size_t>(4));
    EXPECT_FALSE(sl.erase(3));                        // 再删 → false
    EXPECT_FALSE(sl.erase(999));                      // 删不存在 → false

    std::vector<std::pair<int,int>> got = dump(sl);
    ASSERT_EQ(got.size(), static_cast<std::size_t>(4));
    int rest[] = {1, 2, 4, 5};
    for (std::size_t i = 0; i < got.size(); ++i) EXPECT_EQ(got[i].first, rest[i]);
}

// ---- 压力：较多元素乱序插入，全部可查、有序、值正确；删一半后仍自洽 ----
TEST(C6_stress, many_elements_stay_consistent) {
    SkipList sl;
    const int N = 200;
    // 用一个简单的伪随机置换（线性同余）打乱插入顺序。
    unsigned x = 12345u;
    std::vector<int> ks;
    for (int i = 0; i < N; ++i) ks.push_back(i);
    for (int i = N - 1; i > 0; --i) {                // Fisher–Yates 洗牌
        x = x * 1103515245u + 12345u;
        int j = static_cast<int>((x >> 16) % static_cast<unsigned>(i + 1));
        std::swap(ks[i], ks[j]);
    }
    for (int k : ks) sl.insert(k, k + 1000);
    EXPECT_EQ(sl.size(), static_cast<std::size_t>(N));

    // 全部可查、值正确。
    for (int i = 0; i < N; ++i) {
        const int* p = sl.find(i);
        ASSERT_NE(p, nullptr);                       // 骨架 → 第一次就中止
        EXPECT_EQ(*p, i + 1000);
    }
    // 遍历严格升序。
    std::vector<std::pair<int,int>> got = dump(sl);
    ASSERT_EQ(got.size(), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) EXPECT_EQ(got[i].first, i);

    // 删掉所有偶数 key。
    for (int i = 0; i < N; i += 2) EXPECT_TRUE(sl.erase(i));
    EXPECT_EQ(sl.size(), static_cast<std::size_t>(N / 2));
    for (int i = 0; i < N; ++i) {
        if (i % 2 == 0) EXPECT_FALSE(sl.contains(i));
        else            EXPECT_TRUE(sl.contains(i));
    }
}
