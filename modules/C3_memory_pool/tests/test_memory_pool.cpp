// =============================================================================
//  C3 测试文件 —— 检查你的定长内存池写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：allocate 恒返回 nullptr、deallocate/grow_ 为空操作。所有用例都先用
//  ASSERT_NE(p, nullptr) 挡在解引用之前，所以一开始只会变红、不会崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "memory_pool.hpp"

#include <new>      // placement new
#include <vector>

using namespace cppbc;

// 一个有 int + double 的小对象，用来演示"在池内存上构造真实对象"。
struct Widget {
    int    a;
    double b;
};

// ---- 基本分配：返回可用、互不相同的块；placement new 能在其上造对象 ----
TEST(C3_alloc, allocate_distinct_usable_blocks) {
    FixedPool pool(sizeof(Widget));
    EXPECT_GE(pool.block_size(), sizeof(Widget));   // 块大小至少能放下 Widget

    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    ASSERT_NE(p1, nullptr);                          // 骨架返回 nullptr → 在此中止
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);                               // 两次分配是不同的块
    EXPECT_EQ(pool.outstanding(), static_cast<std::size_t>(2));

    Widget* w = new (p1) Widget{7, 2.5};            // placement new：在池内存上构造
    EXPECT_EQ(w->a, 7);
    EXPECT_TRUE(w->b == 2.5);
    w->~Widget();                                    // 手动析构（生内存需手动管理对象生命周期）

    pool.deallocate(p1);
    pool.deallocate(p2);
    EXPECT_EQ(pool.outstanding(), static_cast<std::size_t>(0));
}

// ---- 空闲链表复用：刚归还的块应被下一次分配立刻取回（后进先出）----
TEST(C3_reuse, freed_block_is_reused) {
    // 每片 chunk 只 1 块，计数才好预测：分配→空闲链表空→扩容出 1 块→取走。
    FixedPool pool(sizeof(int), /*blocks_per_chunk=*/1);
    void* p1 = pool.allocate();
    ASSERT_NE(p1, nullptr);
    pool.deallocate(p1);                             // 还回去 → 空闲链表又有 1 块
    void* p2 = pool.allocate();                      // 应复用刚还回去的同一块
    EXPECT_EQ(p1, p2);
    pool.deallocate(p2);
    EXPECT_EQ(pool.free_count(), static_cast<std::size_t>(1));  // 这一块回到空闲链表
    EXPECT_EQ(pool.outstanding(), static_cast<std::size_t>(0)); // 没有未归还的块
}

// ---- 按需扩容：每片 chunk 4 块，第 5 次分配触发批发第 2 片 ----
TEST(C3_grow, grows_by_chunks_on_demand) {
    FixedPool pool(sizeof(int), /*blocks_per_chunk=*/4);

    std::vector<void*> ps;
    for (int i = 0; i < 4; ++i) {
        void* p = pool.allocate();
        ASSERT_NE(p, nullptr);                       // 骨架在这里就会中止
        ps.push_back(p);
    }
    EXPECT_EQ(pool.chunk_count(), static_cast<std::size_t>(1));  // 头 4 块来自第 1 片

    void* fifth = pool.allocate();                   // 第 5 块 → 触发扩容
    ASSERT_NE(fifth, nullptr);
    ps.push_back(fifth);
    EXPECT_EQ(pool.chunk_count(), static_cast<std::size_t>(2));  // 批发了第 2 片
    EXPECT_EQ(pool.outstanding(), static_cast<std::size_t>(5));

    for (void* p : ps) pool.deallocate(p);
    EXPECT_EQ(pool.outstanding(), static_cast<std::size_t>(0));
}
