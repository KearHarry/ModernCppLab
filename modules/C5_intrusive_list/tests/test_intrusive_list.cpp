// =============================================================================
//  C5 测试文件 —— 检查你的侵入式双向链表写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：push_back/unlink 为空操作、pop_front 恒 nullptr。链表始终为空，所有取
//  元素前都用 ASSERT_NE/ASSERT_EQ 挡住，所以一开始只变红、不崩溃。哨兵构造、empty/
//  front/back/size/for_each 已给好。
// =============================================================================
#include "test_framework.hpp"
#include "intrusive_list.hpp"

#include <vector>

using namespace cppbc;

// 一个会入链的小对象：公有继承 ListNode 钩子。
struct Item : ListNode {
    int v;
    explicit Item(int x) : v(x) {}
};

// 收集链表里的值（顺序）。
static std::vector<int> collect(IntrusiveList<Item>& L) {
    std::vector<int> out;
    L.for_each([&](Item& it) { out.push_back(it.v); });
    return out;
}

// ---- 入链保持顺序，front/back 正确 ----
TEST(C5_push, push_back_keeps_order) {
    IntrusiveList<Item> L;
    Item a(1), b(2), c(3);
    EXPECT_TRUE(L.empty());

    L.push_back(a);
    L.push_back(b);
    L.push_back(c);
    EXPECT_FALSE(L.empty());
    EXPECT_EQ(L.size(), static_cast<std::size_t>(3));

    Item* f = L.front();
    Item* bk = L.back();
    ASSERT_NE(f, nullptr);                            // 骨架 push 为空操作 → 链表空 → 在此中止
    ASSERT_NE(bk, nullptr);
    EXPECT_EQ(f->v, 1);
    EXPECT_EQ(bk->v, 3);

    std::vector<int> got = collect(L);
    ASSERT_EQ(got.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(got[0], 1);
    EXPECT_EQ(got[1], 2);
    EXPECT_EQ(got[2], 3);
}

// ---- 核心超能力：只凭对象本身就能 O(1) 摘除，无需遍历查找 ----
TEST(C5_unlink, unlink_is_o1_given_only_element) {
    IntrusiveList<Item> L;
    Item a(10), b(20), c(30);
    L.push_back(a);
    L.push_back(b);
    L.push_back(c);
    ASSERT_EQ(L.size(), static_cast<std::size_t>(3));  // 骨架链表空 → 在此中止

    IntrusiveList<Item>::unlink(b);                    // 只给 b，不需要知道它在链表哪
    EXPECT_EQ(L.size(), static_cast<std::size_t>(2));

    std::vector<int> got = collect(L);
    ASSERT_EQ(got.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(got[0], 10);
    EXPECT_EQ(got[1], 30);                             // B 被跳过，A<->C 直连
    EXPECT_EQ(L.front()->v, 10);
    EXPECT_EQ(L.back()->v, 30);
}

// ---- pop_front 依次摘下首元素，最后为空 ----
TEST(C5_pop, pop_front_drains_in_order) {
    IntrusiveList<Item> L;
    Item a(7), b(8);
    L.push_back(a);
    L.push_back(b);

    Item* p1 = L.pop_front();
    ASSERT_NE(p1, nullptr);                            // 骨架恒 nullptr → 在此中止
    EXPECT_EQ(p1->v, 7);

    Item* p2 = L.pop_front();
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->v, 8);

    EXPECT_TRUE(L.empty());
    EXPECT_EQ(L.pop_front(), nullptr);                 // 空链表再 pop 安全返回 nullptr
}

// ---- 零分配在多个链表间转移：同一对象从 L1 摘下、挂到 L2 ----
TEST(C5_relink, element_moves_between_lists_zero_alloc) {
    IntrusiveList<Item> L1, L2;
    Item a(1), b(2);
    L1.push_back(a);
    L1.push_back(b);
    ASSERT_EQ(L1.size(), static_cast<std::size_t>(2)); // 骨架链表空 → 在此中止

    IntrusiveList<Item>::unlink(a);                    // 从 L1 摘下（只需 a）
    L2.push_back(a);                                   // 挂到 L2——没有任何 new

    EXPECT_EQ(L1.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(L2.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(L1.front()->v, 2);
    EXPECT_EQ(L2.front()->v, 1);
}

// ---- 防御：空链表各接口安全；放一个进去后不再为空 ----
TEST(C5_defensive, empty_list_is_safe) {
    IntrusiveList<Item> L;
    EXPECT_TRUE(L.empty());
    EXPECT_EQ(L.front(), nullptr);
    EXPECT_EQ(L.back(), nullptr);
    EXPECT_EQ(L.pop_front(), nullptr);
    EXPECT_EQ(L.size(), static_cast<std::size_t>(0));

    Item x(99);
    L.push_back(x);                                    // 骨架空操作 → 下面变红
    EXPECT_FALSE(L.empty());
    Item* f = L.front();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->v, 99);
}
