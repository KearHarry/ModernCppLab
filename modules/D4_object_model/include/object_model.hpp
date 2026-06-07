// =============================================================================
//  D4 · 虚函数与对象模型（vtable / vptr / 虚析构 / clone / RTTI）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  通过一个 Shape 抽象基类 + Circle / Rect 派生类，把 C++ **对象模型**里最
//  常考的概念串起来：虚函数怎么实现"运行时多态"、vptr/vtable 是什么、为什么基类
//  析构必须 virtual、抽象类/纯虚函数、虚拷贝(clone/原型模式)、以及 RTTI(dynamic_cast)。
//  这些是"C++ 对象模型"面试的核心，也是理解一切多态框架的地基。
//
//  【虚函数是怎么做到"运行时多态"的？——vptr 与 vtable】
//  只要一个类有虚函数，编译器就给它生成一张 **虚函数表(vtable)**：里面按槽位放着
//  各虚函数的真正地址。每个对象里则埋一个隐藏指针 **vptr**，指向自己所属类型的 vtable。
//
//      Circle 对象            Circle 的 vtable
//      ┌──────────┐          ┌────────────────────┐
//      │  vptr ───┼────────▶ │ [0] Circle::area   │
//      │  r_      │          │ [1] Circle::name   │
//      └──────────┘          │ [2] Circle::clone  │
//                            │ [3] ~Circle (dtor) │
//                            └────────────────────┘
//
//  当你通过基类指针/引用调用 `shape->area()` 时，编译器并不知道 shape 到底指向
//  Circle 还是 Rect，于是生成"**查 vptr → 找到对应 vtable 槽位 → 间接调用**"
//  的代码。对象在构造时 vptr 被设成"自己真实类型"的 vtable，所以运行时就调到了
//  正确的派生版本——这就是**动态分派(dynamic dispatch)**。代价是一次间接跳转
//  （和无法内联），换来运行时多态的灵活性。
//
//  【为什么基类析构函数必须是 virtual？】
//  通过基类指针 delete 一个派生对象时：
//    - 若 ~Base() **不是** virtual：只调用 ~Base()，派生类的析构被跳过 → 派生类持有的
//      资源泄漏，是**未定义行为**。
//    - 若 ~Base() **是** virtual：delete 走 vtable 找到 ~Derived()，先析构派生、再析构基类，
//      顺序正确、资源全部释放。
//  口诀：**「打算被继承、且会通过基类指针删除」的类，析构函数一定要 virtual。**
//
//  【抽象类与纯虚函数】
//  `virtual double area() const = 0;` 中的 `= 0` 表示**纯虚函数**：基类只声明不实现，
//  含纯虚函数的类是**抽象类**，不能实例化（`Shape s;` 编译失败），只能被继承。
//  好处之一：抽象基类天然**防住了"对象切片(slicing)"**——你无法把派生对象按值塞进
//  一个 Shape，因为 Shape 根本不能作为值存在，只能用指针/引用持有。
//
//  【虚拷贝：clone() / 原型模式】
//  有了基类指针，怎么"复制出一个同样真实类型的新对象"？普通拷贝构造做不到
//  （它只认静态类型）。解法是每个派生类重写 `clone()` 返回 `new 自己`，这样
//  `base_ptr->clone()` 就能多态地克隆出正确的派生类型——即**原型模式**。
//
//  【RTTI：dynamic_cast / typeid】
//  运行时类型识别。`dynamic_cast<Circle*>(shape)`：若 shape 实际指向 Circle 就返回
//  有效指针，否则返回 nullptr。它依赖 vtable 里挂的类型信息，仅对多态类型可用。
//
// =============================================================================
#pragma once

#include <cstddef>  // std::size_t
#include <memory>   // std::unique_ptr
#include <vector>   // std::vector

namespace cppbc {

// 圆周率常量（测试与实现共用同一个值，便于精确比较面积）。
inline constexpr double kPi = 3.141592653589793;

// ---------------------------------------------------------------------------
//  抽象基类：只定义"形状应有的行为"，自身不能实例化。
//  注意 ~Shape() 是 virtual —— 这是通过 Shape* 删除派生对象不泄漏的前提。
// ---------------------------------------------------------------------------
class Shape {
public:
    virtual double area() const = 0;                  // 纯虚：面积
    virtual const char* name() const = 0;             // 纯虚：类型名
    virtual std::unique_ptr<Shape> clone() const = 0; // 纯虚：虚拷贝（原型模式）
    virtual ~Shape() = default;                        // 【关键】基类析构必须 virtual！
};

// ---------------------------------------------------------------------------
//  Circle：半径 r_。alive 记录当前存活的 Circle 个数（演示析构是否被正确调用）。
// ---------------------------------------------------------------------------
class Circle : public Shape {
public:
    inline static int alive = 0;                      // 存活计数（教学用）

    explicit Circle(double r) : r_(r) { ++alive; }
    Circle(const Circle& o) : Shape(o), r_(o.r_) { ++alive; }  // 拷贝构造也要计数
    ~Circle() { --alive; }   // 基类 dtor 已 virtual → 此析构自动也是 virtual

    // ===================== TODO(D4-1) Circle 的三个重写 =============
    //   area()  : return kPi * r_ * r_;        // π·r²
    //   name()  : return "Circle";
    //   clone() : return std::make_unique<Circle>(*this);  // 克隆出同类型新对象
    // ===============================================================
    double area() const override {
        // TODO
        return 0.0;
    }
    const char* name() const override {
        // TODO
        return "?";
    }
    std::unique_ptr<Shape> clone() const override {
        // TODO
        return nullptr;
    }

    double radius() const noexcept { return r_; }

private:
    double r_;
};

// ---------------------------------------------------------------------------
//  Rect：宽 w_、高 h_。
// ---------------------------------------------------------------------------
class Rect : public Shape {
public:
    inline static int alive = 0;

    Rect(double w, double h) : w_(w), h_(h) { ++alive; }
    Rect(const Rect& o) : Shape(o), w_(o.w_), h_(o.h_) { ++alive; }  // 拷贝构造也要计数
    ~Rect() { --alive; }

    // ===================== TODO(D4-2) Rect 的三个重写 =========
    //   area()  : return w_ * h_;
    //   name()  : return "Rect";
    //   clone() : return std::make_unique<Rect>(*this);
    // ===============================================================
    double area() const override {
        // TODO
        return 0.0;
    }
    const char* name() const override {
        // TODO
        return "?";
    }
    std::unique_ptr<Shape> clone() const override {
        // TODO
        return nullptr;
    }

    double width()  const noexcept { return w_; }
    double height() const noexcept { return h_; }

private:
    double w_, h_;
};

// ===================== TODO(D4-3) total_area =====================
// 多态地累加一组形状的面积：通过基类指针调用 area()，每个都分派到真实类型。
//   double sum = 0.0;
//   for (const auto& s : shapes) sum += s->area();
//   return sum;
// ===============================================================
inline double total_area(const std::vector<std::unique_ptr<Shape>>& shapes) {
    // TODO
    (void)shapes;
    return 0.0;
}

// ===================== TODO(D4-4) circle_radius_or ===============
// 用 RTTI 判断 s 的真实类型：若确实是 Circle 就返回它的半径，否则返回 fallback。
//   if (const Circle* c = dynamic_cast<const Circle*>(&s))
//       return c->radius();
//   return fallback;
// ===============================================================
inline double circle_radius_or(const Shape& s, double fallback) {
    // TODO
    (void)s;
    return fallback;
}

} // namespace cppbc
