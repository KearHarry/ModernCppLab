// =============================================================================
//  E2 测试文件 —— 检查你的线性/帧分配器写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：allocate 恒返回 nullptr、reset/rewind 为空操作。所有解引用/比较前都先用
//  ASSERT_NE 挡住，所以一开始只变红、不崩溃。used()/capacity()/marker() 已给好。
// =============================================================================
#include "test_framework.hpp"
#include "linear_allocator.hpp"

#include <cstdint>

using namespace cppbc;

// ---- 基本：连续分配出互不相同的块，used 随之增长 ----
TEST(E2_basic, allocate_distinct_blocks) {
    LinearAllocator a(1024);
    EXPECT_EQ(a.capacity(), static_cast<std::size_t>(1024));
    EXPECT_EQ(a.used(), static_cast<std::size_t>(0));

    void* p1 = a.allocate(64);
    void* p2 = a.allocate(64);
    ASSERT_NE(p1, nullptr);                          // 骨架返回 nullptr → 在此中止
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);                               // 两次分配是不同的块
    EXPECT_GE(a.used(), static_cast<std::size_t>(128));
    EXPECT_LE(a.used(), a.capacity());
}

// ---- 对齐：请求 16 字节对齐时，返回地址必须对齐 ----
TEST(E2_align, respects_alignment) {
    LinearAllocator a(1024);
    void* p0 = a.allocate(1);                        // 先吃掉 1 字节，让游标变"不整"
    ASSERT_NE(p0, nullptr);
    void* p = a.allocate(8, 16);                     // 请求 16 字节对齐
    ASSERT_NE(p, nullptr);
    auto addr = reinterpret_cast<std::uintptr_t>(p);
    EXPECT_EQ(addr % 16u, static_cast<std::uintptr_t>(0));   // 地址确实 16 对齐
}

// ---- 重置：reset 后游标归零，内存从头复用 ----
TEST(E2_reset, reset_reuses_from_start) {
    LinearAllocator a(256);
    void* first = a.allocate(32);
    ASSERT_NE(first, nullptr);
    a.allocate(32);
    EXPECT_GE(a.used(), static_cast<std::size_t>(64));

    a.reset();
    EXPECT_EQ(a.used(), static_cast<std::size_t>(0));
    void* again = a.allocate(32);                    // 应复用最开始那一块
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again, first);
}

// ---- 越界：容量不足时返回 nullptr，且失败的分配不消耗空间 ----
TEST(E2_overflow, out_of_space_returns_null) {
    LinearAllocator a(64);
    void* p = a.allocate(48);
    ASSERT_NE(p, nullptr);
    std::size_t before = a.used();
    void* big = a.allocate(1000);                    // 远超剩余空间
    EXPECT_EQ(big, nullptr);                          // 分配失败
    EXPECT_EQ(a.used(), before);                      // 失败不能推游标
}

// ---- 栈式回收：marker + rewind 把临时作用域的内存一次性吐回（后进先出）----
TEST(E2_rewind, marker_rewind_reclaims) {
    LinearAllocator a(256);
    void* base = a.allocate(16);
    ASSERT_NE(base, nullptr);

    LinearAllocator::Marker m = a.marker();
    void* scratch = a.allocate(64);                  // 进入一段临时作用域
    ASSERT_NE(scratch, nullptr);
    EXPECT_GT(a.used(), static_cast<std::size_t>(m));

    a.rewind(m);                                     // 弹出该作用域
    EXPECT_EQ(a.used(), static_cast<std::size_t>(m));
    void* reuse = a.allocate(64);                    // 应交还同一片 scratch 区域
    ASSERT_NE(reuse, nullptr);
    EXPECT_EQ(reuse, scratch);
}

// ---- 便捷构造：create<T> 在 arena 上 placement new 出真实对象 ----
TEST(E2_create, placement_construct_on_arena) {
    LinearAllocator a(256);
    struct Vec3 { float x, y, z; };
    Vec3* v = a.create<Vec3>(Vec3{1.0f, 2.0f, 3.0f});
    ASSERT_NE(v, nullptr);                           // 骨架 allocate 失败 → 在此中止
    EXPECT_TRUE(v->x == 1.0f);
    EXPECT_TRUE(v->y == 2.0f);
    EXPECT_TRUE(v->z == 3.0f);
    // POD 类型无需析构，reset 直接整片丢弃即可。
    a.reset();
    EXPECT_EQ(a.used(), static_cast<std::size_t>(0));
}
