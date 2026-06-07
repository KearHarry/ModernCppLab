// =============================================================================
//  D5 测试文件 —— 检查你的 Pimpl Widget 写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：构造函数不分配 Impl，读取方法返回默认值（0 / 空串）。于是下面的「值」
//  用例会红（sum/count/name 都不对），但不会崩溃（方法没去解引用空指针）。
//  唯一「天生为绿」的是布局用例——它验证的是 Pimpl 的结构不变量，与你填没填 TODO 无关。
//  把 src/widget.cpp 的 3 个 TODO 填上，其余用例即转绿。
// =============================================================================
#include "test_framework.hpp"
#include "widget.hpp"

#include <string>
#include <utility>

using namespace cppbc;

// ---- 基本委托：push 进去的数，sum/count 要数得准；name 要存得住 ----
TEST(D5_basic, push_sum_count_name) {
    Widget w("cfg");
    EXPECT_EQ(w.name(), std::string("cfg"));
    EXPECT_EQ(w.count(), 0u);
    EXPECT_EQ(w.sum(), 0);

    w.push(3);
    w.push(4);
    w.push(10);
    EXPECT_EQ(w.count(), 3u);
    EXPECT_EQ(w.sum(), 17);
}

// ---- 编译防火墙的结构证据：Widget 只有「一个指针」那么大，与 Impl 多臃肿无关 ----
TEST(D5_layout, widget_is_one_pointer) {
    EXPECT_EQ(sizeof(Widget), sizeof(void*));
}

// ---- 移动构造：把内部 Impl 指针整体转移给新对象（O(1)，不深拷贝数据）----
TEST(D5_move, move_construct_transfers) {
    Widget a("src");
    a.push(1);
    a.push(2);
    a.push(3);

    Widget b(std::move(a));                       // 把 a 的 Impl 偷给 b
    EXPECT_EQ(b.name(), std::string("src"));
    EXPECT_EQ(b.count(), 3u);
    EXPECT_EQ(b.sum(), 6);
    // 注意：a 现在是「已移动」状态（内部指针为空），除了析构或重新赋值外，不应再用它。
}

// ---- 移动赋值：把一个 Widget 的内容搬进另一个已存在的 Widget ----
TEST(D5_move, move_assign_transfers) {
    Widget a("A");
    a.push(5);
    a.push(5);
    Widget b("B");
    b.push(100);

    b = std::move(a);                             // b 原来的 Impl 被释放，接管 a 的
    EXPECT_EQ(b.name(), std::string("A"));
    EXPECT_EQ(b.count(), 2u);
    EXPECT_EQ(b.sum(), 10);
}
