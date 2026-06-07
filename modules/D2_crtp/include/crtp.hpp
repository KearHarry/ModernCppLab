// =============================================================================
//  D2 · CRTP 静态多态（Curiously Recurring Template Pattern）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  用 **CRTP** 做一套"形状"的多态——但**不用虚函数、不用 vtable**，全部在**编译期**
//  完成分派。把它和 D4（虚函数运行期多态）对照着学，你就能回答大厂高频题：
//  "除了虚函数，C++ 还能怎么实现多态？""CRTP 是什么、解决什么、代价是什么？"
//
//  【什么是 CRTP？一句话：基类把自己的"派生类型"当模板参数拿到手】
//      template <class Derived>
//      struct Base { ... 用 static_cast<Derived*>(this) 调用派生的实现 ... };
//
//      struct Circle : Base<Circle> { ... };   // 注意：基类的模板参数就是自己！
//                  ▲────────────┘  "Curiously Recurring"（奇异递归）就指这里
//
//  基类 `Base<Derived>` 在编译期就**知道**自己真正的派生类型是谁，于是可以
//  `static_cast<Derived*>(this)->impl()` 直接调到派生版本——**编译期就绑定好**，
//  没有运行期查表。这叫**静态多态 / 编译期多态**。
//
//  【和 D4 虚函数的对照（核心考点）】
//                      虚函数（D4）              CRTP（D2）
//      绑定时机        运行期（查 vptr/vtable）   编译期（static_cast 直接定位）
//      每对象开销      多一个 vptr 指针           零额外成员，sizeof 不变
//      调用开销        间接跳转、通常无法内联     直接调用、**可内联**（常更快）
//      异构容器        可以：vector<Base*> 混装   不行：Base<Circle> 与 Base<Rect> 是不同类型
//      二进制接口      稳定（加派生类不改基类）   模板，改动易引发重编译
//      典型用途        运行期才知道类型、插件     类型编译期已知、追求极致性能（CRTP/表达式模板）
//
//  一句话取舍：**需要"运行期把不同类型装进同一个容器"用虚函数；类型编译期已知、又想
//  省掉虚调用开销，用 CRTP。** 这也解释了为什么 STL/Eigen 等高性能库偏爱 CRTP。
//
//  【CRTP 的第二大用途：注入式 mixin（给派生类"白送"一套能力）】
//  本文件还给了一个 `Counted<Derived>` 例子：任何类只要 `: Counted<自己>`，就自动获得
//  "统计本类型存活对象数"的能力。关键妙处——`Counted<Circle>` 和 `Counted<Rect>` 是
//  **两个不同的类型**，各自有**独立的** `static int alive`。于是每个派生类拿到的是
//  **专属于自己**的计数器，互不干扰。这种"用 CRTP 把通用能力按类型注入"的手法，在
//  `std::enable_shared_from_this`、各种 mixin 库里随处可见。
//
//  【你会实现的 3 个 TODO】
//    D2-1  CrtpShape<Derived>::area  —— 用 static_cast 分派到派生的 area_impl()
//    D2-2  CrtpShape<Derived>::name  —— 用 static_cast 分派到派生的 name_impl()
//    D2-3  sum_areas(同构容器)        —— 编译期分派地累加一批同类型形状的面积
//  （Counted 注入式计数器作为"已写好的范例"给出，配套测试一开始就是绿的，供你研读。）
//
// =============================================================================
#pragma once

#include <vector>  // std::vector

namespace cppbc {

// 圆周率（与测试共用，便于精确比较面积）。
inline constexpr double kPi = 3.141592653589793;

// ---------------------------------------------------------------------------
//  CRTP 基类：提供统一的 area()/name() 接口，内部静态分派到派生的 *_impl()。
//  注意它**没有任何虚函数**，也没有数据成员——零运行期开销。
// ---------------------------------------------------------------------------
template <class Derived>
struct CrtpShape {
    // ===================== TODO(D2-1) area 静态分派 =================
    //  把调用"转交"给真正的派生实现——这就是 CRTP 的核心机关：
    //      return static_cast<const Derived*>(this)->area_impl();
    //  （this 实际指向某个派生对象；向下转型为 Derived* 后调它的 area_impl()，
    //   全程编译期确定，无 vtable）
    // ===============================================================
    double area() const {
        // TODO
        return 0.0;
    }

    // ===================== TODO(D2-2) name 静态分派 =================
    //      return static_cast<const Derived*>(this)->name_impl();
    // ===============================================================
    const char* name() const {
        // TODO
        return "?";
    }

    // 【已给好】基类复用派生行为的范例：缩放面积 = k × area()。
    //  area() 一旦实现，这个"写在基类、却用到派生实现"的算法就自动对所有派生类生效——
    //  这正是 CRTP 用来做"算法复用 / 接口注入"的价值所在。
    double scaled_area(double k) const { return k * area(); }
};

// ---------------------------------------------------------------------------
//  Circle：CRTP 派生类。把自己 CircleS 作为模板参数传给 CrtpShape。
//  同时 mix-in 一个 Counted<CircleS>，白得一个"专属于 CircleS"的存活计数器。
// ---------------------------------------------------------------------------
template <class Derived>
struct Counted {
    inline static int alive = 0;          // 每个 Derived 都有自己**独立**的这一份
    Counted()                  { ++alive; }
    Counted(const Counted&)    { ++alive; }
    Counted(Counted&&) noexcept{ ++alive; }
    ~Counted()                 { --alive; }
};

struct CircleS : CrtpShape<CircleS>, Counted<CircleS> {
    double r;
    explicit CircleS(double radius) : r(radius) {}

    // 供 CRTP 基类静态分派调用的"真正实现"（已给好）。
    double      area_impl() const { return kPi * r * r; }  // π·r²
    const char* name_impl() const { return "Circle"; }
};

struct RectS : CrtpShape<RectS>, Counted<RectS> {
    double w, h;
    RectS(double width, double height) : w(width), h(height) {}

    double      area_impl() const { return w * h; }
    const char* name_impl() const { return "Rect"; }
};

// ===================== TODO(D2-3) sum_areas =====================
//  累加"同一种"形状的面积。注意：因为是同构容器 vector<Derived>，每次 .area() 都在
//  编译期分派到 Derived 的实现、可内联——这正是 CRTP 相对虚函数的性能优势所在。
//  （对照 D4 的 total_area：那里是 vector<unique_ptr<Shape>> 异构容器 + 运行期虚分派。）
//      double sum = 0.0;
//      for (const auto& s : items) sum += s.area();
//      return sum;
// ===============================================================
template <class Derived>
double sum_areas(const std::vector<Derived>& items) {
    // TODO
    (void)items;
    return 0.0;
}

} // namespace cppbc
