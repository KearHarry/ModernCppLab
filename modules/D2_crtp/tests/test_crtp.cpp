// =============================================================================
//  D2 测试文件 —— 检查你的 CRTP 静态多态写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：area 恒 0、name 为 "?"、sum_areas 为 0，所以前 3 个用例会变红、但不崩溃
//  （静态分派的占位返回都是安全值）。最后一个 D2_mixin 用例验证的是"已写好"的 Counted
//  注入式计数器，从一开始就是绿的——它是给你研读的范例，不是你要实现的 TODO。
// =============================================================================
#include "test_framework.hpp"
#include "crtp.hpp"

#include <cmath>
#include <cstring>
#include <vector>

using namespace cppbc;

static bool close(double a, double b) { return std::fabs(a - b) < 1e-9; }

// ---- 静态多态：基类 area()/name() 在编译期分派到派生实现 ----
TEST(D2_static_dispatch, base_dispatches_to_derived) {
    CircleS c(1.0);                              // r=1 → 面积 π
    EXPECT_TRUE(close(c.area(), kPi));
    EXPECT_EQ(std::strcmp(c.name(), "Circle"), 0);

    RectS r(2.0, 3.0);                           // 面积 6
    EXPECT_TRUE(close(r.area(), 6.0));
    EXPECT_EQ(std::strcmp(r.name(), "Rect"), 0);
}

// ---- 基类算法复用派生行为：scaled_area 内部调用 area() ----
TEST(D2_reuse, base_algorithm_reuses_derived) {
    CircleS c(1.0);
    EXPECT_TRUE(close(c.scaled_area(2.0), 2.0 * kPi));   // 2 × π
}

// ---- 同构容器 + 编译期分派累加面积（对照 D4 的异构虚分派）----
TEST(D2_sum, homogeneous_container) {
    std::vector<RectS> rects;
    rects.push_back(RectS(2.0, 3.0));            // 6
    rects.push_back(RectS(4.0, 5.0));            // 20
    EXPECT_TRUE(close(sum_areas(rects), 26.0));
}

// ---- CRTP mixin：每个派生类型拥有独立的 alive 计数器（已写好的范例，应为绿）----
TEST(D2_mixin, per_type_independent_counter) {
    CircleS::alive = 0;
    RectS::alive   = 0;
    {
        CircleS c1(1.0), c2(2.0);
        RectS   r1(1.0, 1.0);
        EXPECT_EQ(CircleS::alive, 2);            // CircleS 与 RectS 计数互相独立
        EXPECT_EQ(RectS::alive, 1);
    }
    EXPECT_EQ(CircleS::alive, 0);                // 离开作用域，析构令计数归零
    EXPECT_EQ(RectS::alive, 0);
}
