// =============================================================================
//  E3 测试文件 —— 检查你的多播信号/委托写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：connect 返回 0 且不挂槽、disconnect 恒 false、emit 为空操作。这里没有裸
//  指针解引用，所以骨架只会"红"（断言失败）而不会崩溃。size/empty/clear 已给好。
// =============================================================================
#include "test_framework.hpp"
#include "signal.hpp"

using namespace cppbc;

// ---- 基本：连接一个槽，emit 后它被调用、参数正确传入 ----
TEST(E3_basic, connect_and_emit_calls_slot) {
    Signal<int> sig;
    int received = 0;
    Signal<int>::Id id = sig.connect([&](int v) { received = v; });
    EXPECT_NE(id, static_cast<Signal<int>::Id>(0));   // 拿到有效连接号
    EXPECT_EQ(sig.size(), static_cast<std::size_t>(1));

    sig.emit(42);
    EXPECT_EQ(received, 42);                            // 回调确实被触发、参数对
}

// ---- 多播：emit 一次，所有已连接的槽都被触发（fan-out）----
TEST(E3_multi, emit_fans_out_to_all_slots) {
    Signal<> sig;                                      // 无参信号
    int calls = 0;
    sig.connect([&] { ++calls; });
    sig.connect([&] { ++calls; });
    sig.connect([&] { ++calls; });
    EXPECT_EQ(sig.size(), static_cast<std::size_t>(3));

    sig.emit();
    EXPECT_EQ(calls, 3);                               // 三个槽全被叫到
}

// ---- 断开：disconnect 精确移除一个槽，其余不受影响；重复断开返回 false ----
TEST(E3_disconnect, disconnect_removes_one_slot) {
    Signal<int> sig;
    int a = 0, b = 0;
    Signal<int>::Id ida = sig.connect([&](int v) { a += v; });
    Signal<int>::Id idb = sig.connect([&](int v) { b += v; });
    EXPECT_EQ(sig.size(), static_cast<std::size_t>(2));

    EXPECT_TRUE(sig.disconnect(ida));                  // 移除 a 的槽
    EXPECT_EQ(sig.size(), static_cast<std::size_t>(1));

    sig.emit(10);
    EXPECT_EQ(a, 0);                                   // a 不再接收
    EXPECT_EQ(b, 10);                                  // b 仍接收

    EXPECT_FALSE(sig.disconnect(ida));                 // 已断开，再断开 → false
    (void)idb;
}

// ---- 多参数：emit 的参数被原样转发给回调 ----
TEST(E3_args, forwards_multiple_arguments) {
    Signal<int, int> sig;
    int sum = -1;
    sig.connect([&](int x, int y) { sum = x + y; });
    sig.emit(3, 4);
    EXPECT_EQ(sum, 7);
}

// ---- 防御：空信号 emit 安全；clear 一次性断开全部 ----
TEST(E3_lifecycle, empty_emit_safe_and_clear_works) {
    Signal<int> sig;
    EXPECT_TRUE(sig.empty());
    sig.emit(1);                                       // 无槽 → 安全空操作
    EXPECT_EQ(sig.size(), static_cast<std::size_t>(0));

    sig.connect([](int) {});
    sig.connect([](int) {});
    EXPECT_EQ(sig.size(), static_cast<std::size_t>(2));

    sig.clear();
    EXPECT_TRUE(sig.empty());                          // 全部断开
}

// ---- Id 唯一：不同连接拿到不同的连接号 ----
TEST(E3_unique_id, connections_get_distinct_ids) {
    Signal<> sig;
    Signal<>::Id id1 = sig.connect([] {});
    Signal<>::Id id2 = sig.connect([] {});
    EXPECT_NE(id1, static_cast<Signal<>::Id>(0));
    EXPECT_NE(id2, static_cast<Signal<>::Id>(0));
    EXPECT_NE(id1, id2);                               // 两个连接号互不相同
}
