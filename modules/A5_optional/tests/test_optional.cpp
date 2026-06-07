// =============================================================================
//  A5 测试文件 —— 检查你的 Optional<T> 写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：reset 为空操作、emplace 不真正构造、拷贝构造为空体。于是 Optional 永远"空"，
//  访问内容前都用 ASSERT_TRUE(has_value()) 挡住，所以一开始只变红、不崩溃。value()/
//  operator*/value_or/移动/赋值 已给好。
// =============================================================================
#include "test_framework.hpp"
#include "optional.hpp"

using namespace cppbc;

// 统计存活实例数的类型，用来验证"构造一次、析构一次、不泄漏不重复析构"。
struct Tracked {
    static int alive;
    int v;
    explicit Tracked(int x) : v(x) { ++alive; }
    Tracked(const Tracked& o) : v(o.v) { ++alive; }
    Tracked(Tracked&& o) noexcept : v(o.v) { ++alive; }
    ~Tracked() { --alive; }
};
int Tracked::alive = 0;

// ---- 基本：默认空；emplace 后有值，可解引用 ----
TEST(A5_basic, empty_then_emplace) {
    Optional<int> o;
    EXPECT_FALSE(o.has_value());
    EXPECT_FALSE(static_cast<bool>(o));

    int& r = o.emplace(42);
    ASSERT_TRUE(o.has_value());                      // 骨架 emplace 不构造 → 在此中止
    EXPECT_TRUE(static_cast<bool>(o));
    EXPECT_EQ(*o, 42);
    EXPECT_EQ(r, 42);
    r = 7;                                           // emplace 返回的引用可写
    EXPECT_EQ(*o, 7);
}

// ---- 构造方式：值构造、隐式构造、拷贝、移动 ----
TEST(A5_ctors, value_copy_move) {
    Optional<int> a(100);
    ASSERT_TRUE(a.has_value());                      // 骨架值构造委托 emplace → 不构造 → 中止
    EXPECT_EQ(*a, 100);

    Optional<int> b = 9;                             // 隐式从 T 构造
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(*b, 9);

    Optional<int> c(a);                              // 拷贝构造
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*c, 100);
    *c = 55;                                         // 深拷贝：改 c 不影响 a
    EXPECT_EQ(*a, 100);
    EXPECT_EQ(*c, 55);

    Optional<int> empty;
    Optional<int> d(empty);                          // 拷贝一个空的 → 仍空
    EXPECT_FALSE(d.has_value());

    Optional<int> e(std::move(a));                   // 移动构造
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(*e, 100);
}

// ---- 赋值：拷贝赋值 / 移动赋值 / 用空覆盖 ----
TEST(A5_assign, copy_move_and_clear) {
    Optional<int> a, b;
    a.emplace(1);
    b.emplace(2);
    ASSERT_TRUE(a.has_value());                      // 骨架 → 中止
    ASSERT_TRUE(b.has_value());

    a = b;                                           // 拷贝赋值
    EXPECT_EQ(*a, 2);

    Optional<int> c;
    c = std::move(b);                                // 移动赋值
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*c, 2);

    Optional<int> empty;
    a = empty;                                       // 用空覆盖 → 变空
    EXPECT_FALSE(a.has_value());
}

// ---- reset：清空后无值；重复 reset 安全 ----
TEST(A5_reset, reset_clears) {
    Optional<int> o;
    o.emplace(5);
    ASSERT_TRUE(o.has_value());                      // 骨架 → 中止

    o.reset();
    EXPECT_FALSE(o.has_value());
    o.reset();                                       // 再次 reset 安全
    EXPECT_FALSE(o.has_value());
}

// ---- 受检访问与 value_or ----
TEST(A5_value_access, value_throws_and_value_or) {
    Optional<int> e;
    EXPECT_THROW(e.value(), BadOptionalAccess);      // 空 → 抛异常
    EXPECT_EQ(e.value_or(-1), -1);                   // 空 → 返回默认

    Optional<int> o;
    o.emplace(3);
    ASSERT_TRUE(o.has_value());                      // 骨架 → 中止
    EXPECT_NO_THROW(o.value());
    EXPECT_EQ(o.value(), 3);
    EXPECT_EQ(o.value_or(-1), 3);                    // 有值 → 返回值本身
}

// ---- 生命周期：构造一次/析构一次，作用域结束不泄漏、不重复析构 ----
TEST(A5_lifetime, constructs_and_destroys_exactly_once) {
    Tracked::alive = 0;
    {
        Optional<Tracked> o;
        EXPECT_EQ(Tracked::alive, 0);                // 空 → 没有活着的对象
        o.emplace(7);
        ASSERT_TRUE(o.has_value());                  // 骨架 emplace 不构造 → 在此中止
        EXPECT_EQ(Tracked::alive, 1);                // 恰构造 1 个
        EXPECT_EQ((*o).v, 7);

        o.reset();
        EXPECT_EQ(Tracked::alive, 0);                // 析构掉了

        o.emplace(8);
        EXPECT_EQ(Tracked::alive, 1);
    }                                                // 作用域结束 → 析构函数 reset
    EXPECT_EQ(Tracked::alive, 0);                    // 不泄漏、不重复析构
}
