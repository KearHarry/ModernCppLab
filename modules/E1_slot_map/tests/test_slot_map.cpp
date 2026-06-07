// =============================================================================
//  E1 测试文件 —— 检查你的 Slot Map / 生成式句柄写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：insert 返回无效句柄、get 恒 nullptr、erase 恒 false。所有解引用前都用
//  ASSERT_TRUE/ASSERT_NE 挡住，所以一开始只变红、不崩溃。valid() 已给好。
// =============================================================================
#include "test_framework.hpp"
#include "slot_map.hpp"

#include <vector>

using namespace cppbc;

// ---- 基本：插入→取出→计数 ----
TEST(E1_basic, insert_get_size) {
    SlotMap<int> sm;
    EXPECT_TRUE(sm.empty());

    Handle h = sm.insert(42);
    EXPECT_TRUE(sm.valid(h));
    int* p = sm.get(h);
    ASSERT_TRUE(p != nullptr);                      // 骨架 get 返回 nullptr → 在此中止
    EXPECT_EQ(*p, 42);
    EXPECT_EQ(sm.size(), static_cast<std::size_t>(1));
}

// ---- 核心：删除后代数自增，旧句柄失效，且槽位复用不会"张冠李戴" ----
TEST(E1_stale, generation_invalidates_old_handle) {
    SlotMap<int> sm;
    Handle h1 = sm.insert(100);
    ASSERT_TRUE(sm.valid(h1));                       // 骨架 insert 返回无效句柄 → 在此中止

    EXPECT_TRUE(sm.erase(h1));
    EXPECT_FALSE(sm.valid(h1));                      // 旧句柄立刻失效
    EXPECT_EQ(sm.get(h1), nullptr);

    Handle h2 = sm.insert(200);                      // 复用同一下标，但代数 +1
    EXPECT_EQ(h2.index, h1.index);                   // 下标确实被复用
    EXPECT_NE(h2.generation, h1.generation);         // 代数不同 → 新旧句柄区分得开
    EXPECT_FALSE(sm.valid(h1));                       // 老句柄仍失效，不会误指向新对象

    int* p2 = sm.get(h2);
    ASSERT_TRUE(p2 != nullptr);
    EXPECT_EQ(*p2, 200);
}

// ---- 多对象：句柄各不相同、各自可查；删中间一个不影响其它 ----
TEST(E1_many, distinct_handles_and_lookup) {
    SlotMap<int> sm;
    std::vector<Handle> hs;
    for (int i = 0; i < 5; ++i) hs.push_back(sm.insert(i * 10));
    EXPECT_EQ(sm.size(), static_cast<std::size_t>(5));

    for (int i = 0; i < 5; ++i) {
        int* p = sm.get(hs[i]);
        ASSERT_TRUE(p != nullptr);
        EXPECT_EQ(*p, i * 10);
    }

    EXPECT_TRUE(sm.erase(hs[2]));
    EXPECT_FALSE(sm.valid(hs[2]));
    EXPECT_EQ(sm.size(), static_cast<std::size_t>(4));

    int* p3 = sm.get(hs[3]);                         // 其它句柄不受影响
    ASSERT_TRUE(p3 != nullptr);
    EXPECT_EQ(*p3, 30);
}

// ---- 防御：二次删除、越界句柄都安全返回 false / nullptr ----
TEST(E1_defensive, double_erase_and_bad_handle) {
    SlotMap<int> sm;
    Handle h = sm.insert(7);
    ASSERT_TRUE(sm.valid(h));

    EXPECT_TRUE(sm.erase(h));
    EXPECT_FALSE(sm.erase(h));                       // 已删，再删返回 false
    EXPECT_EQ(sm.size(), static_cast<std::size_t>(0));

    Handle bogus{12345u, 0u};                        // 完全不存在的下标
    EXPECT_FALSE(sm.valid(bogus));
    EXPECT_FALSE(sm.erase(bogus));
    EXPECT_EQ(sm.get(bogus), nullptr);
}
