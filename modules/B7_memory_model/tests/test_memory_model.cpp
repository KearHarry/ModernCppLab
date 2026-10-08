// =============================================================================
//  B7 测试：验证发布协议的可观察结果与引用计数状态机。
//  测试不含故意的数据竞争，也不把偶现重排当作判题依据。
// =============================================================================
#include "test_framework.hpp"
#include "memory_model.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace cppbc;

// 功能运行结果无法揭示弱内存序是否写错，因此先把每个协议的策略当作编译期契约检查。
static_assert(memory_order_policy::kPublishStore == std::memory_order_release);
static_assert(memory_order_policy::kConsumeLoad == std::memory_order_acquire);
static_assert(memory_order_policy::kFlagRelaxed == std::memory_order_relaxed);
static_assert(memory_order_policy::kReleaseFence == std::memory_order_release);
static_assert(memory_order_policy::kAcquireFence == std::memory_order_acquire);
static_assert(memory_order_policy::kRefIncrement == std::memory_order_relaxed);
static_assert(memory_order_policy::kRefDecrement == std::memory_order_release);
static_assert(memory_order_policy::kCountObserve == std::memory_order_relaxed);

TEST(B7_release_acquire, unread_mailbox_is_safe) {
    ReleaseAcquireMailbox box;
    int out = -1;
    EXPECT_FALSE(box.try_consume(out));
    EXPECT_EQ(out, -1); // 未发布时不得改输出值。
}

TEST(B7_release_acquire, published_payload_is_visible) {
    ReleaseAcquireMailbox box;
    std::thread producer([&] { box.publish(42); });
    producer.join();

    int out = -1;
    EXPECT_TRUE(box.try_consume(out)); // 骨架不发布，在这里变红。
    EXPECT_EQ(out, 42);
    EXPECT_TRUE(box.ready_relaxed());
}

TEST(B7_fence, fence_protocol_publishes_payload) {
    FenceMailbox box;
    box.publish(2026);
    int out = -1;
    EXPECT_TRUE(box.try_consume(out)); // 骨架 false → 红。
    EXPECT_EQ(out, 2026);
}

TEST(B7_fence, empty_mailbox_does_not_touch_output) {
    FenceMailbox box;
    int out = 99;
    EXPECT_FALSE(box.try_consume(out));
    EXPECT_EQ(out, 99);
}

TEST(B7_happens_before, synchronization_is_transitive) {
    HappensBeforeChain chain;
    chain.publish(314);
    EXPECT_TRUE(chain.relay()); // 骨架 false → 红。
    int out = 0;
    EXPECT_TRUE(chain.try_consume(out));
    EXPECT_EQ(out, 314);
}

TEST(B7_ref_count, add_and_release_protocol) {
    AtomicRefCount refs;
    EXPECT_EQ(refs.use_count(), std::size_t{1});
    refs.add_ref();
    refs.add_ref();
    EXPECT_EQ(refs.use_count(), std::size_t{3}); // 骨架仍为 1 → 红。

    EXPECT_FALSE(refs.release_ref());
    EXPECT_FALSE(refs.release_ref());
    EXPECT_TRUE(refs.release_ref());             // 最后一份引用负责销毁。
}

TEST(B7_ref_count, concurrent_retain_release_keeps_owner_alive) {
    AtomicRefCount refs;
    constexpr int kThreads = 8;
    constexpr int kEach = 1000;

    std::vector<std::thread> workers;
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < kEach; ++i) refs.add_ref();
        });
    }
    for (auto& worker : workers) worker.join();
    EXPECT_EQ(refs.use_count(), std::size_t{1 + kThreads * kEach});

    std::atomic<int> premature_last{0};
    workers.clear();
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < kEach; ++i) {
                if (refs.release_ref()) premature_last.fetch_add(1);
            }
        });
    }
    for (auto& worker : workers) worker.join();

    EXPECT_EQ(premature_last.load(), 0); // 创建者的那份引用仍在。
    EXPECT_EQ(refs.use_count(), std::size_t{1});
    EXPECT_TRUE(refs.release_ref());
}
