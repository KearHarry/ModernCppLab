// =============================================================================
//  D5 · Pimpl —— 实现文件（你在这里填 TODO）
// -----------------------------------------------------------------------------
//  关键：Impl 的「完整定义」只出现在这个 .cpp 里。头文件只有前置声明，所以
//  ~Widget()/移动操作**必须在这里定义**——此处 Impl 完整，unique_ptr 才能正确
//  析构它。把它们留给头文件默认生成 = 在 Impl 不完整处 delete = 报错/UB。
//
//  骨架现状：构造函数暂时**不分配** Impl（p_ 为空），各读取方法返回默认值，于是
//  能编译、能链接、不崩溃——但读到的值不对，测试会红。把下面 3 个 TODO 填上即转绿。
// =============================================================================
#include "widget.hpp"

#include <numeric>   // std::accumulate
#include <utility>   // std::move
#include <vector>

namespace cppbc {

// ---- Impl：被隐藏的真实数据（调用方完全看不到，改它不波及任何 #include 方）----
struct Widget::Impl {
    std::string      name;
    std::vector<int> data;
};

// ===================== TODO(D5-1) 构造函数 =====================
//  在堆上造一个 Impl、把 name 存进去（用成员初始化列表初始化 p_）：
//      Widget::Widget(std::string name)
//          : p_(std::make_unique<Impl>(Impl{ std::move(name), {} })) {}
// =============================================================
Widget::Widget(std::string name) {
    (void)name;          // 骨架：暂不分配 Impl，p_ 保持为空（方法因而读到默认值）
}

// —— 三个特殊成员：必须定义在「这里」（Impl 已完整）。骨架已给好正确版本。——
//    想体会那个经典坑？把下面三行删掉、改去 widget.hpp 里写 `~Widget() = default;`，
//    立刻收获一串「invalid application of sizeof / delete of incomplete type」报错。
Widget::~Widget()                            = default;
Widget::Widget(Widget&&) noexcept            = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

// ===================== TODO(D5-2) push =========================
//  往内部容器追加一个数：
//      p_->data.push_back(v);
// =============================================================
void Widget::push(int v) {
    (void)v;             // 骨架：什么都不做
}

// ===================== TODO(D5-3) 三个读取方法 =================
//  全部委托给 p_ 去读内部状态：
//      int                sum()   -> std::accumulate(p_->data.begin(), p_->data.end(), 0)
//      std::size_t        count() -> p_->data.size()
//      const std::string& name()  -> p_->name
// =============================================================
int Widget::sum() const {
    return 0;            // 骨架：恒 0
}

std::size_t Widget::count() const {
    return 0;            // 骨架：恒 0
}

const std::string& Widget::name() const {
    static const std::string kEmpty;   // 骨架：p_ 为空，先返回一个静态空串占位
    return kEmpty;
}

} // namespace cppbc
