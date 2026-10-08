#include "test_framework.hpp"
#include "allocator_pmr.hpp"

#include <cstdint>
#include <list>
#include <memory_resource>
#include <string>
#include <vector>

using namespace cppbc;

TEST(C9_allocator, vector_records_and_balances_bytes) {
    auto stats = std::make_shared<AllocationStats>();
    {
        std::vector<int, CountingAllocator<int>> values{
            CountingAllocator<int>{stats}};
        for (int i = 0; i < 64; ++i) values.push_back(i);
        EXPECT_GT(stats->allocation_calls, std::size_t{0});
        EXPECT_GT(stats->bytes_outstanding, std::size_t{0});
        EXPECT_EQ(values[31], 31);
    }
    EXPECT_EQ(stats->bytes_outstanding, std::size_t{0});
    EXPECT_EQ(stats->allocation_calls, stats->deallocation_calls);
}

TEST(C9_allocator, list_rebind_shares_state) {
    auto stats = std::make_shared<AllocationStats>();
    {
        std::list<std::string, CountingAllocator<std::string>> words{
            CountingAllocator<std::string>{stats}};
        words.emplace_back("allocator");
        words.emplace_back("traits");
        words.emplace_back("rebind");
        // 标准不规定 list 必须逐节点调用一次 allocate；只验证 rebind 后确实
        // 使用了同一份 allocator 状态，不绑定某个标准库的节点分配策略。
        EXPECT_GT(stats->allocation_calls, std::size_t{0});
        EXPECT_EQ(words.size(), std::size_t{3});
    }
    EXPECT_EQ(stats->bytes_outstanding, std::size_t{0});
}

TEST(C9_allocator, equality_is_resource_identity) {
    auto shared = std::make_shared<AllocationStats>();
    CountingAllocator<int> a{shared};
    CountingAllocator<double> rebound{a};
    CountingAllocator<int> independent;
    EXPECT_TRUE(a == rebound);
    EXPECT_FALSE(a == independent);
}

struct alignas(128) CacheLineBlock { char data[128]{}; };

TEST(C9_pmr, resource_preserves_over_alignment) {
    CountingResource resource;
    {
        std::pmr::vector<CacheLineBlock> blocks{&resource};
        blocks.resize(4);
        auto address = reinterpret_cast<std::uintptr_t>(blocks.data());
        EXPECT_EQ(address % alignof(CacheLineBlock), std::uintptr_t{0});
        EXPECT_GT(resource.stats().allocation_calls, std::size_t{0});
        EXPECT_GT(resource.stats().bytes_outstanding, std::size_t{0});
    }
    EXPECT_EQ(resource.stats().bytes_outstanding, std::size_t{0});
}

TEST(C9_pmr, polymorphic_allocator_uses_selected_resource) {
    CountingResource first;
    CountingResource second;
    {
        std::pmr::string a{&first};
        std::pmr::string b{&second};
        a.assign(200, 'a');
        b.assign(300, 'b');
        EXPECT_GT(first.stats().bytes_allocated, std::size_t{0});
        EXPECT_GT(second.stats().bytes_allocated, std::size_t{0});
        EXPECT_FALSE(first.is_equal(second));
    }
    EXPECT_EQ(first.stats().bytes_outstanding, std::size_t{0});
    EXPECT_EQ(second.stats().bytes_outstanding, std::size_t{0});
}
