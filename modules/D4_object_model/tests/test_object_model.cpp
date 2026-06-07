// =============================================================================
//  D4 测试文件 —— 检查你的 object_model.hpp 写对了没有
// -----------------------------------------------------------------------------
//  说明：面积是浮点数，用 close() 做容差比较；clone() 返回空指针的骨架用例先用
//  ASSERT_TRUE 挡住，避免解引用空 unique_ptr。骨架阶段 area 恒 0、name 为 "?"、
//  clone 为 nullptr、total_area 为 0、RTTI 恒返回 fallback，所以一开始只会变红、不崩溃。
// =============================================================================
#include "test_framework.hpp"
#include "object_model.hpp"

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

using namespace cppbc;

// 浮点容差比较（标准库 string/数值不保证位精确，面积是乘法结果，用容差最稳）。
static bool close(double a, double b) { return std::fabs(a - b) < 1e-9; }

// ---- 动态分派：通过基类指针调用，分派到派生类的 area()/name() ----
TEST(D4_dispatch, virtual_call_through_base_pointer) {
    std::unique_ptr<Shape> s = std::make_unique<Circle>(2.0);
    EXPECT_TRUE(close(s->area(), kPi * 4.0));            // π·2² = 4π
    EXPECT_EQ(std::strcmp(s->name(), "Circle"), 0);

    std::unique_ptr<Shape> r = std::make_unique<Rect>(3.0, 4.0);
    EXPECT_TRUE(close(r->area(), 12.0));                 // 3×4
    EXPECT_EQ(std::strcmp(r->name(), "Rect"), 0);
}

// ---- 多态容器：基类指针装不同派生类，统一累加面积 ----
TEST(D4_poly, total_area_over_mixed_container) {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Rect>(2.0, 3.0));   // 6
    shapes.push_back(std::make_unique<Rect>(4.0, 5.0));   // 20
    shapes.push_back(std::make_unique<Circle>(1.0));           // π

    EXPECT_TRUE(close(total_area(shapes), 6.0 + 20.0 + kPi));
}

// ---- 虚拷贝(clone/原型)：克隆出"同样真实类型"的新对象；且虚析构正确回收 ----
TEST(D4_clone, clone_preserves_type_and_destroys) {
    Circle::alive = 0;
    {
        std::unique_ptr<Shape> orig = std::make_unique<Circle>(2.0);
        std::unique_ptr<Shape> copy = orig->clone();
        ASSERT_TRUE(copy != nullptr);                   // 骨架 clone 返回 nullptr → 在此中止
        EXPECT_EQ(Circle::alive, 2);                    // 原件 + 克隆，两个 Circle 存活
        EXPECT_EQ(std::strcmp(copy->name(), "Circle"), 0);   // 克隆仍是 Circle
        EXPECT_TRUE(close(copy->area(), orig->area()));      // 面积一致
        EXPECT_NE(copy.get(), orig.get());              // 是独立的新对象
    } // 两个对象通过 Shape* 销毁 → 虚析构调用 ~Circle，各自 --alive
    EXPECT_EQ(Circle::alive, 0);                         // 全部正确回收
}

// ---- RTTI：dynamic_cast 识别真实类型 ----
TEST(D4_rtti, dynamic_cast_identifies_type) {
    Circle    c(5.0);
    Rect r(2.0, 3.0);
    const Shape& sc = c;
    const Shape& sr = r;

    EXPECT_TRUE(close(circle_radius_or(sc, -1.0), 5.0));   // 是 Circle → 返回半径 5
    EXPECT_TRUE(close(circle_radius_or(sr, -1.0), -1.0));  // 是 Rect → 返回 fallback
}
