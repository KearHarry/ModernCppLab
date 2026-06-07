// =============================================================================
//  D5 · Pimpl 编译防火墙（Pointer to IMPLementation）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  演示 **Pimpl** 手法：把一个类的**私有实现**全部搬到 .cpp 里，头文件只留下
//  「一个指向不完整类型 Impl 的指针 + 公开方法的声明」。好处是——
//    · 改私有实现只需重编那**一个 .cpp**，而不是所有 #include 了本头的文件；
//    · 私有依赖（第三方库的头、笨重的容器）**不外泄**给调用方；
//    · 类的体积恒为「一个指针」，**ABI 稳定**：加减私有成员不动头、不破坏二进制接口。
//  这就是「编译防火墙」。代价是一次堆分配 + 一次间接寻址、方法难以跨 .cpp 内联。
//
//  【整个类只有一个数据成员】
//      std::unique_ptr<Impl> p_;     // Impl 的真身藏在 .cpp，这里只有前置声明
//  调用方包含本头时，只看见 Widget 的接口，**看不到** Impl 长什么样、依赖了谁。
//
//  【⚠ 头文件最容易踩的坑：析构/移动不能让编译器在「这里」默认生成】
//  unique_ptr<Impl> 销毁时要 `delete` 一个 Impl；而 `delete` 一个**不完整类型**是
//  未定义行为（标准库还会用 static_assert 直接把你拦下来编译报错）。若把 ~Widget()
//  交给编译器在**头文件**里默认生成，那一刻 Impl 还**不完整** → 报错。
//  解法：在头里只**声明** ~Widget() 和移动操作，把**定义**放到 .cpp——
//  在 .cpp 里 Impl 已经完整，`delete` 合法。（定义体写 `= default` 也行，重点是
//  「定义点」在 .cpp，不在头。）
//
//  【你会实现的 3 个 TODO（都在 src/widget.cpp）】
//    D5-1  构造函数        —— 在堆上 make_unique 出 Impl，把 name 存进去
//    D5-2  push            —— 往 Impl 内部的容器追加一个数
//    D5-3  sum/count/name  —— 委托给 p_ 读取内部状态
//  （本头文件是「公开接口」，骨架与参考答案**完全一致**，无需改动——Pimpl 的精髓
//    正是「接口头稳定不变，只动 .cpp」。）
//
// =============================================================================
#pragma once

#include <cstddef>   // std::size_t
#include <memory>    // std::unique_ptr
#include <string>

namespace cppbc {

class Widget {
public:
    explicit Widget(std::string name);        // 构造：在 .cpp 里 new 出 Impl

    // —— 三个特殊成员：此处只「声明」，定义统一放到 .cpp（那里 Impl 才完整）——
    ~Widget();                                // 不能让编译器在头里默认生成（见导读）
    Widget(Widget&&) noexcept;                // 移动构造：把 Impl 指针整体转移
    Widget& operator=(Widget&&) noexcept;     // 移动赋值

    Widget(const Widget&)            = delete; // 本模块不做深拷贝，禁掉
    Widget& operator=(const Widget&) = delete; // （想拷贝得在 .cpp 里 new 一份 Impl）

    // —— 公开接口：调用方只看见这些，看不到任何实现细节/私有依赖 ——
    void               push(int v);           // 往内部容器追加一个数
    int                sum()   const;         // 内部所有数之和
    std::size_t        count() const;         // 内部元素个数
    const std::string& name()  const;         // 名字

private:
    struct Impl;                              // 前置声明：不完整类型，真身藏在 .cpp
    std::unique_ptr<Impl> p_;                 // 唯一数据成员 —— 编译防火墙
};

} // namespace cppbc
