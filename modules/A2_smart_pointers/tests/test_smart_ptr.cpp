// =============================================================================
//  A2 测试文件 —— 检查你的 smart_ptr.hpp 写对了没有
// -----------------------------------------------------------------------------
//  小技巧：测试里用一个"存活计数器" Counted 来观察对象有没有被正确销毁——
//  构造时 ++alive，析构时 --alive。只要某段代码结束后 alive 归 0，
//  就说明没有泄漏、也没有提前释放。
// =============================================================================
#include "test_framework.hpp"
#include "smart_ptr.hpp"

#include <type_traits>
#include <utility>

using namespace cppbc;

// 存活计数器：用来"看见"对象的构造与析构。
struct Counted {
    static int alive;
    int value;
    explicit Counted(int v = 0) : value(v) { ++alive; }
    ~Counted() { --alive; }
};
int Counted::alive = 0;

// =============================================================================
//  UniquePtr
// =============================================================================

// 编译期：UniquePtr 必须不可拷贝（独占语义）。
static_assert(!std::is_copy_constructible_v<UniquePtr<int>>,
              "UniquePtr 不应可拷贝");
static_assert(!std::is_copy_assignable_v<UniquePtr<int>>,
              "UniquePtr 不应可拷贝赋值");
static_assert(std::is_move_constructible_v<UniquePtr<int>>,
              "UniquePtr 应当可移动");

TEST(A2_unique, owns_and_frees_on_scope_exit) {
    ASSERT_EQ(Counted::alive, 0);
    {
        UniquePtr<Counted> p(new Counted(7));
        EXPECT_EQ(Counted::alive, 1);
        EXPECT_TRUE(static_cast<bool>(p));
        EXPECT_EQ(p->value, 7);
        EXPECT_EQ((*p).value, 7);
    }
    EXPECT_EQ(Counted::alive, 0);   // 离开作用域，析构应释放对象
}

TEST(A2_unique, move_transfers_ownership) {
    UniquePtr<Counted> a(new Counted(1));
    UniquePtr<Counted> b = std::move(a);          // 移动构造
    EXPECT_EQ(a.get(), static_cast<Counted*>(nullptr));  // a 交出了所有权
    ASSERT_TRUE(b.get() != nullptr);
    EXPECT_EQ(b->value, 1);
    EXPECT_EQ(Counted::alive, 1);                 // 仍只有一个对象，没被重复创建/删除
}

TEST(A2_unique, release_gives_up_without_deleting) {
    Counted* raw = nullptr;
    {
        UniquePtr<Counted> p(new Counted(5));
        raw = p.release();                        // 放弃所有权但不删除
        EXPECT_EQ(p.get(), static_cast<Counted*>(nullptr));
    }
    EXPECT_EQ(Counted::alive, 1);                 // p 析构没删它，对象还在
    delete raw;                                   // 由我们手动删
    EXPECT_EQ(Counted::alive, 0);
}

TEST(A2_unique, reset_replaces_and_frees) {
    UniquePtr<Counted> p(new Counted(1));
    EXPECT_EQ(Counted::alive, 1);
    p.reset(new Counted(2));                      // 释放旧的，接管新的
    EXPECT_EQ(Counted::alive, 1);
    EXPECT_EQ(p->value, 2);
    p.reset();                                    // 释放，变空
    EXPECT_EQ(Counted::alive, 0);
    EXPECT_FALSE(static_cast<bool>(p));
}

// =============================================================================
//  SharedPtr
// =============================================================================

TEST(A2_shared, use_count_tracks_owners) {
    ASSERT_EQ(Counted::alive, 0);
    {
        SharedPtr<Counted> a(new Counted(42));
        EXPECT_EQ(a.use_count(), 1L);
        EXPECT_EQ(a->value, 42);
        {
            SharedPtr<Counted> b = a;             // 拷贝构造 → 计数 +1
            EXPECT_EQ(a.use_count(), 2L);
            EXPECT_EQ(b.use_count(), 2L);
            EXPECT_EQ(Counted::alive, 1);         // 还是同一个对象
        }
        EXPECT_EQ(a.use_count(), 1L);             // b 离开作用域 → 计数 -1
        EXPECT_EQ(Counted::alive, 1);
    }
    EXPECT_EQ(Counted::alive, 0);                 // 最后一个 shared 走了 → 销毁对象
}

TEST(A2_shared, move_and_copy_assign) {
    SharedPtr<Counted> a(new Counted(1));
    SharedPtr<Counted> b = std::move(a);          // 移动：a 变空，计数不变
    EXPECT_EQ(a.use_count(), 0L);
    EXPECT_EQ(b.use_count(), 1L);

    SharedPtr<Counted> c(new Counted(2));
    EXPECT_EQ(Counted::alive, 2);
    c = b;                                        // 拷贝赋值：c 先放掉自己的(2)，再共享 b 的(1)
    EXPECT_EQ(Counted::alive, 1);                 // 那个值为 2 的对象被销毁
    EXPECT_EQ(b.use_count(), 2L);
    EXPECT_EQ(c->value, 1);
}

TEST(A2_shared, self_assignment_is_safe) {
    SharedPtr<Counted> a(new Counted(9));
    a = a;                                        // 自赋值不应销毁自己
    ASSERT_TRUE(a.get() != nullptr);
    EXPECT_EQ(a->value, 9);
    EXPECT_EQ(a.use_count(), 1L);
    EXPECT_EQ(Counted::alive, 1);
}

// =============================================================================
//  WeakPtr
// =============================================================================

TEST(A2_weak, lock_while_alive_succeeds) {
    SharedPtr<Counted> s(new Counted(3));
    WeakPtr<Counted> w = s;                       // 从 shared 构造 weak
    EXPECT_FALSE(w.expired());
    SharedPtr<Counted> s2 = w.lock();             // 提升成功
    ASSERT_TRUE(static_cast<bool>(s2));
    EXPECT_EQ(s2->value, 3);
    EXPECT_EQ(s.use_count(), 2L);                 // s + s2
}

TEST(A2_weak, does_not_keep_object_alive) {
    WeakPtr<Counted> w;
    {
        SharedPtr<Counted> s(new Counted(1));
        w = s;
        EXPECT_EQ(Counted::alive, 1);
        EXPECT_FALSE(w.expired());
    }
    // shared 已全部离开 → 对象应被销毁，weak 不应延长其寿命
    EXPECT_EQ(Counted::alive, 0);
    EXPECT_TRUE(w.expired());
    EXPECT_FALSE(static_cast<bool>(w.lock()));    // 提升失败，返回空
}

// 循环引用：a 强引用 b，b 用【弱引用】指回 a → 环被打破，无泄漏。
struct Node {
    static int alive;
    SharedPtr<Node> next;   // 强引用
    WeakPtr<Node>   prev;   // 弱引用（若改成 SharedPtr 就会泄漏）
    Node()  { ++alive; }
    ~Node() { --alive; }
};
int Node::alive = 0;

TEST(A2_weak, breaks_reference_cycle) {
    ASSERT_EQ(Node::alive, 0);
    {
        SharedPtr<Node> a = MakeShared<Node>();
        SharedPtr<Node> b = MakeShared<Node>();
        a->next = b;            // a 强引用 b
        b->prev = a;            // b 弱引用 a（不增加 a 的 strong 计数）
        EXPECT_EQ(Node::alive, 2);
        EXPECT_EQ(a.use_count(), 1L);   // 只有局部变量 a 在强持有 a
        EXPECT_EQ(b.use_count(), 2L);   // 局部 b + a->next 在强持有 b
    }
    EXPECT_EQ(Node::alive, 0);          // 两个节点都被正确回收，无泄漏
}
