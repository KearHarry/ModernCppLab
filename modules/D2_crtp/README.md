# D2 · CRTP 静态多态（Curiously Recurring Template Pattern）

难度 ⭐⭐⭐ · 预计 2h

## 为什么大厂爱考
"除了虚函数，C++ 还能怎么实现多态？"——能答出 **CRTP（编译期多态）** 并讲清它与虚函数的取舍，是区分"会用继承"和"懂 C++ 多态全貌"的标志。CRTP 是 STL、Eigen、各种高性能库的常用手法（省掉虚调用、可内联）。把它和 D4（虚函数运行期多态）对照学，多态这条线就完整了。

## 背景知识

### 1) 什么是 CRTP：基类把"派生类型"当模板参数拿到手
```cpp
template <class Derived>
struct Base { /* 用 static_cast<Derived*>(this) 调派生实现 */ };

struct Circle : Base<Circle> { ... };   // 基类的模板参数就是自己 → "奇异递归"
```
基类 `Base<Derived>` 在**编译期**就知道真正的派生类型，于是能 `static_cast<Derived*>(this)->impl()` 直接调到派生版本——**编译期绑定，无运行期查表**。这就是**静态多态/编译期多态**。

### 2) 和虚函数（D4）的对照（核心考点）
| 维度 | 虚函数（D4） | CRTP（D2） |
|------|-------------|-----------|
| 绑定时机 | 运行期（查 vptr/vtable） | 编译期（`static_cast` 直接定位） |
| 每对象开销 | 多一个 vptr | 零额外成员，`sizeof` 不变 |
| 调用开销 | 间接跳转，通常不可内联 | 直接调用，**可内联**（常更快） |
| 异构容器 | 可以：`vector<Base*>` 混装 | 不行：`Base<Circle>` 与 `Base<Rect>` 是不同类型 |
| 二进制接口/编译 | 稳定，加派生类不动基类 | 模板，改动易触发大量重编译 |
| 适用场景 | 运行期才知道类型、插件系统 | 类型编译期已知、追求极致性能 |

一句话：**要"运行期把不同类型装进同一容器"用虚函数；类型编译期已知又想省虚调用，用 CRTP。**

### 3) CRTP 的第二大用途：注入式 mixin
任何类 `: Counted<自己>` 就自动获得"统计本类型存活对象数"的能力。妙在 `Counted<Circle>` 与 `Counted<Rect>` 是**两个不同类型**，各有**独立**的 `static int alive`——每个派生类拿到**专属**计数器，互不干扰。`std::enable_shared_from_this`、各类 mixin 库都用这招"按类型注入能力"。

### 4) 为什么 `static_cast` 是安全的
CRTP 里 `static_cast<Derived*>(this)` 永远成立：因为对象**真的**是 `Derived`（继承关系如此），不是在赌类型。这与 D4 的 `dynamic_cast`（运行期试探、可能失败）本质不同——CRTP 的向下转型编译期就保证正确，零成本。

## 你要实现什么
打开 [include/crtp.hpp](include/crtp.hpp)。派生类 `CircleS/RectS` 的数据与 `*_impl()`、基类的 `scaled_area`、`Counted` mixin 都已给好。你实现 3 个 `TODO`：

| 编号 | 位置 | 任务 |
|------|------|------|
| D2-1 | `CrtpShape<Derived>::area` | `static_cast<const Derived*>(this)->area_impl()` |
| D2-2 | `CrtpShape<Derived>::name` | `static_cast<const Derived*>(this)->name_impl()` |
| D2-3 | `sum_areas(items)` | 遍历同构容器，编译期分派累加 `area()` |

## 关键坑
- **`static_cast` 的方向**：把基类 `this` 转成**派生** `Derived*`（向下转型）。因为对象本就是 `Derived`，这是安全且零成本的；不要用 `dynamic_cast`（那是运行期、且需要虚函数）。
- **CRTP 基类的成员函数是"延迟实例化"的**：`area()` 体内用到 `Derived::area_impl()`，而 `Derived` 在 `: CrtpShape<Derived>` 那行其实还不完整——没关系，成员函数体到**被调用时**才实例化，那时 `Derived` 已完整。这正是 CRTP 能成立的语言机制。
- **不能做异构容器**：`CrtpShape<CircleS>` 和 `CrtpShape<RectS>` 是不同类型，没法像 D4 那样塞进一个 `vector`。想混装不同形状仍得回到虚函数——这是 CRTP 的根本限制。
- **`Counted` 的 `alive` 是每类型独立的**：`CircleS::alive` 与 `RectS::alive` 互不相干，别以为是同一个。

## 如何验证
```powershell
ctest --test-dir build -R D2 --output-on-failure
```
测试覆盖：基类 `area()/name()` **编译期分派**到派生实现；基类算法 `scaled_area` **复用**派生行为；**同构容器** `sum_areas` 累加（对照 D4 的异构虚分派）；CRTP **mixin** 让每个类型获得独立计数器。

> 提示：骨架里 `area` 恒 0、`name` 为 "?"、`sum_areas` 为 0，前 3 个用例一开始变红、但不崩溃。最后的 `D2_mixin` 验证的是**已写好**的 `Counted` 范例，从一开始就是绿的——它供你研读 CRTP 的第二种用法，不是你要实现的 TODO。

## 面试追问

1. 除了虚函数，C++ 还有哪些实现多态的方式？CRTP 属于哪一类（静态多态）？

   **答**：C++ 多态分两大类。**动态多态**（运行期）：虚函数/继承、`std::function`/类型擦除、函数指针。**静态多态**（编译期）：模板（鸭子类型）、CRTP、重载/特化、C++20 concepts、`if constexpr`。**CRTP**（Curiously Recurring Template Pattern，`class D : Base<D>`）属于**静态多态**——基类在编译期就通过模板参数知道派生类型，用 `static_cast` 向下转调用派生方法，分派在编译期完成、无虚表。

2. CRTP 相比虚函数省掉了什么开销？为什么能内联？什么场景这点性能很关键？

   **答**：CRTP 省掉虚函数的两项运行期开销：①**每次调用一次虚表间接跳转**（读 vptr→查 vtable→跳转）；②对象里的 **vptr 空间**。更关键的是调用目标在**编译期已确定**，编译器能**内联**派生实现并跨调用做优化（常量传播、循环展开）；虚调用因目标运行期才定，通常无法内联。性能关键场景：**热路径上海量小对象的频繁调用**——数值计算（Eigen 表达式模板）、图形/游戏内循环、容器适配层，省掉间接跳转 + 可内联能带来数量级差异。

3. CRTP 的根本限制是什么？（不能异构容器、模板膨胀、编译依赖）何时必须回到虚函数？

   **答**：根本限制：①**不能装进同一容器**——`Base<D1>` 和 `Base<D2>` 是**不同类型**、无共同基类指针，无法 `vector<Base*>` 装不同派生类型（这正是虚函数强项）；②**模板代码膨胀**——每个派生类型实例化一份基类代码，二进制变大、编译变慢；③**编译期耦合**——调用点必须见完整模板定义，接口实现都在头文件、改一处全量重编。**何时回到虚函数**：需要运行期才知道的类型、要把不同类型装进同一容器、要跨 DLL/ABI 边界、要减少编译依赖（配合 Pimpl）时——即"运行期多态"本质需求时。

4. CRTP 基类里 `static_cast<Derived*>(this)` 为什么安全？和 `dynamic_cast` 有何本质区别？

   **答**：安全是因为 CRTP 的**使用约定**保证 `Derived` 真的从 `Base<Derived>` 继承（`class Derived : Base<Derived>`），所以这个 `Base` 子对象**确实是某个 `Derived` 对象的基类子对象**，向下转指向真实派生对象、无误。与 `dynamic_cast` 的本质区别：`static_cast` 在**编译期**按类型关系算地址偏移、**零运行期开销**、但不检查（转错是 UB）；`dynamic_cast` 在**运行期**查 RTTI 验证真实类型、失败返回 nullptr/抛异常、**有开销且要求多态类型（有虚函数）**。CRTP 因类型编译期已知且约定正确，用零开销的 static_cast 即可。

5. 为什么 CRTP 基类的成员函数能调用"此时尚不完整的派生类"的方法？（成员函数延迟实例化）

   **答**：因为 C++ 的**类模板成员函数是"延迟实例化"的**——`Base<Derived>` 的成员函数体只在**被调用时**才实例化，而那时 `Derived` 已是完整类型。类定义阶段编译器只解析基类模板声明、不实例化用到 `Derived` 成员的函数体，所以"基类里调用此刻尚不完整的 Derived 方法"不报错；等某处真正调用该基类成员函数时，`Derived` 定义已全可见，`static_cast<Derived*>(this)->foo()` 得以正确实例化。这条"成员函数按需延迟实例化"规则正是 CRTP 能成立的语言基础。

6. `Counted<Derived>` 为什么每个派生类型有独立的 `alive`？这和模板实例化机制有什么关系？

   **答**：因为 `Counted<Derived>` 是**类模板**，每个不同的 `Derived` 都会实例化出一个**独立类型** `Counted<A>`、`Counted<B>`……而静态成员 `alive` 是"每个类模板实例一份"。所以 `Counted<A>::alive` 和 `Counted<B>::alive` 是**两个互不相干的变量**，A 的计数不影响 B。这正是模板实例化机制的体现：模板不是一份代码共享静态数据，而是**按模板实参各生成一套**，静态成员随之各有一份。利用这点，CRTP mixin 能给每个派生类型"白送"一个独立计数器/注册表，无需各自手写。

7. 你在哪些真实库里见过 CRTP？（`enable_shared_from_this`、Eigen 表达式模板、`std::ranges::view_interface`……）

   **答**：①**`std::enable_shared_from_this<T>`**——`class T : enable_shared_from_this<T>`，让对象安全拿到指向自己的 `shared_ptr`，最经典。②**Eigen** 的表达式模板——矩阵表达式继承 `MatrixBase<Derived>`，编译期组合运算、消除临时对象。③**`std::ranges::view_interface<D>`**（C++20）——视图只要实现 `begin/end`，CRTP 基类自动补齐 `empty/size/front/operator[]`。④还有 LLVM 的 RTTI/pass 体系、Boost.Iterator 的 `iterator_facade`、各种 mixin/policy 基类。它们都用 CRTP 把"通用骨架"塞进基类、把"具体行为"留给派生，零开销复用。

8. CRTP 和 C++20 concepts / 普通模板函数（鸭子类型）相比，各自适合表达什么样的"接口约束"？

   **答**：三者都做"编译期接口约束"，侧重不同。**CRTP**：基类**提供可复用的默认实现/骨架**（NVI、自动补齐接口、注入计数器），是"**继承式**的静态接口 + 代码复用"，适合"我给一套模板方法、你填几个钩子"。**普通模板函数（鸭子类型）**：不声明约束、"能用就行"，最灵活但**报错晦涩、约束隐式**，适合简单泛型算法。**C++20 concepts**：把"类型需满足哪些操作"**显式命名、可读、参与重载决议、报错清晰**，是"**契约式**的约束声明"，但不提供实现。组合用法：concept 约束模板参数（说清要求）、CRTP 提供共享实现（减少重复）、鸭子类型兜底简单场景。
