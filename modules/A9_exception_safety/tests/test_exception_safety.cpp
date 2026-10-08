// =============================================================================
//  A9 测试：用显式 throw 和“第 N 次拷贝抛出”的类型验证 RAII 与强保证。
//  所有失败都是确定性的，不依赖内存不足等不可控条件。
// =============================================================================
#include "test_framework.hpp"
#include "exception_safety.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

using namespace cppbc;

struct CopyBomb {
    inline static int alive = 0;
    inline static int copies_before_throw = -1;

    int value = 0;
    explicit CopyBomb(int v = 0) : value(v) { ++alive; }
    CopyBomb(const CopyBomb& other) : value(other.value) {
        if (copies_before_throw == 0) throw std::runtime_error("copy bomb");
        if (copies_before_throw > 0) --copies_before_throw;
        ++alive;
    }
    CopyBomb(CopyBomb&& other) noexcept : value(other.value) { ++alive; }
    CopyBomb& operator=(const CopyBomb&) = default;
    CopyBomb& operator=(CopyBomb&&) = default;
    ~CopyBomb() { --alive; }
};

TEST(A9_scope_exit, runs_once_on_normal_exit) {
    int cleanups = 0;
    {
        auto guard = make_scope_exit([&cleanups]() noexcept { ++cleanups; });
        EXPECT_TRUE(guard.active());
        EXPECT_EQ(cleanups, 0);
    }
    EXPECT_EQ(cleanups, 1);
}

TEST(A9_scope_exit, runs_during_exception_unwinding) {
    int cleanups = 0;
    try {
        auto guard = make_scope_exit([&cleanups]() noexcept { ++cleanups; });
        (void)guard;
        throw std::runtime_error("work failed");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(cleanups, 1);
}

TEST(A9_scope_exit, release_cancels_cleanup) {
    int cleanups = 0;
    {
        auto guard = make_scope_exit([&cleanups]() noexcept { ++cleanups; });
        guard.release();
        EXPECT_FALSE(guard.active());
    }
    EXPECT_EQ(cleanups, 0);
}

TEST(A9_scope_exit, move_transfers_exactly_once) {
    int cleanups = 0;
    {
        auto first = make_scope_exit([&cleanups]() noexcept { ++cleanups; });
        auto second = std::move(first);
        EXPECT_FALSE(first.active());
        EXPECT_TRUE(second.active());
    }
    EXPECT_EQ(cleanups, 1);
}

TEST(A9_transaction, commits_successful_operation) {
    TransactionalVector<int> values{1, 2};
    values.transact([](std::vector<int>& draft) {
        draft.push_back(3);
        draft[0] = 10;
    });

    ASSERT_EQ(values.size(), std::size_t{3});
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST(A9_transaction, rollback_when_operation_throws) {
    TransactionalVector<int> values{1, 2};
    EXPECT_THROW(values.transact([](std::vector<int>& draft) {
        draft[0] = 99;
        draft.push_back(3);
        throw std::runtime_error("abort transaction");
    }), std::runtime_error);

    ASSERT_EQ(values.size(), std::size_t{2});
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
}

TEST(A9_append, appends_entire_batch_on_success) {
    TransactionalVector<int> values{1, 2};
    values.append_all_strong(std::vector<int>{3, 4, 5});
    ASSERT_EQ(values.size(), std::size_t{5});
    EXPECT_EQ(values[2], 3);
    EXPECT_EQ(values[4], 5);
}

TEST(A9_append, injected_copy_failure_preserves_original_and_lifetime) {
    CopyBomb::alive = 0;
    CopyBomb::copies_before_throw = -1;
    {
        std::vector<CopyBomb> initial;
        initial.emplace_back(1);
        initial.emplace_back(2);
        TransactionalVector<CopyBomb> values(std::move(initial));

        std::vector<CopyBomb> additions;
        additions.emplace_back(3);
        additions.emplace_back(4);
        const int alive_before = CopyBomb::alive;

        CopyBomb::copies_before_throw = 1; // 确定地在事务复制期间失败
        EXPECT_THROW(values.append_all_strong(additions), std::runtime_error);
        CopyBomb::copies_before_throw = -1;

        ASSERT_EQ(values.size(), std::size_t{2});
        EXPECT_EQ(values[0].value, 1);
        EXPECT_EQ(values[1].value, 2);
        EXPECT_EQ(CopyBomb::alive, alive_before); // 临时副本已全部清理
    }
    EXPECT_EQ(CopyBomb::alive, 0);
}
