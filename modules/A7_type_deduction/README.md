# A7 · 类型推导（auto / decltype / 模板推导 / 退化）

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考

C++ 的类型常常不是“写出来的那个类型”。`auto` 会丢掉顶层 `const`，模板按值传参会让数组退化成指针，`decltype(x)` 与 `decltype((x))` 甚至只差一对括号却可能分别得到 `T` 和 `T&`。这些规则直接决定重载选择、完美转发、返回值生命周期和泛型库是否正确，也是面试中最容易连续追问的语言核心。

本模块不用会让错误实现直接编译失败的 `static_assert` 当主判题手段，而是把编译期类型映射成运行期 `TypeTag`。这样可以逐项实现、逐项观察结果；尚未覆盖的类型会落到 `Unknown`，不会引发未定义行为。

## 背景知识

### 1) `auto` 的规则接近模板按值推导

```cpp
const int x = 1;
auto a = x;        // int：顶层 const 丢失
const auto b = x;  // const int：显式加回来
auto& c = x;       // const int&：引用推导保留底层 const
```

顶层 `const` 修饰对象自身；底层 `const` 修饰指针/引用所指对象。按值复制出新对象时，原对象自身是否 const 通常不重要，因此顶层 const 被丢掉。

### 2) 三种模板形参的推导不同

- `f(T)`：按值；丢顶层 cv，数组和函数退化成指针。
- `f(T&)`：左值引用；保留 cv，数组和函数不退化。
- `f(T&&)` 且 `T` 被推导：转发引用；左值令 `T=U&`，右值令 `T=U`。

转发引用最终依赖引用折叠：“一左则左，全部为右才是右”。

### 3) 数组与函数退化

数组表达式在多数按值场景会从 `T[N]` 转成 `T*`；函数会从函数类型转成函数指针。`sizeof`、取地址以及绑定到引用等上下文不会发生这种退化。面试里“数组传参为什么拿不到长度”就是这条规则。

### 4) `decltype` 有两套规则

- 对无括号的名字或成员访问，`decltype(name)` 直接给出它的声明类型。
- 对其它表达式，按值类别决定：左值得 `T&`，将亡值得 `T&&`，纯右值得 `T`。

所以局部变量 `int x` 有 `decltype(x)==int`，但 `decltype((x))==int&`。

### 5) `decltype(auto)` 保留表达式的精确类型

普通 `auto` 返回值按值推导，引用会被剥掉；`decltype(auto)` 按 `decltype` 规则推导，可原样返回引用。不过 `return x;` 和 `return (x);` 仍可能不同，而且绝不能返回局部变量引用。

## 你要实现什么

打开 [include/type_deduction.hpp](include/type_deduction.hpp)，完成 6 组 `TODO`：

| 编号 | 任务 | 核心知识点 |
|------|------|------------|
| A7-1 | `type_tag<T>()` | `if constexpr`、cv/ref、数组与函数类型 |
| A7-2 | `deduce_by_value` | 按值推导、顶层 const 丢失、退化 |
| A7-3 | `deduce_by_lref` | 引用推导保留 cv/数组/函数 |
| A7-4 | `deduce_by_forward` | 转发引用与引用折叠 |
| A7-5 | `element` | `decltype(auto)` 与引用返回 |
| A7-6 | 两个 decltype 探针 | `decltype(name)` vs `decltype((expr))` |

## 关键坑

- `auto&&` 不总是右值引用；发生推导时它也可能是转发引用。
- `const auto* p` 的 const 修饰所指对象，`auto* const p` 才是指针本身 const。
- 字符串字面量是 `const char[N]`，不是 `const char*`；只是常在按值上下文退化。
- `decltype(auto)` 很强，但返回局部对象的 `(local)` 会产生悬垂引用。
- `std::decay_t<T>` 不只移除引用/cv，还会主动做数组、函数退化；`remove_cvref_t` 不会。
- 本模块用标签观测类型，不代表类型推导发生在运行期；真实推导仍全部在编译期完成。

## 如何验证

```powershell
cmake --build build -j
ctest --test-dir build -R '^A7_' --output-on-failure
```

测试覆盖顶层 const、三种模板形参、数组/函数退化、转发引用、`decltype(auto)` 引用返回，以及括号改变 `decltype` 结果。

## 面试追问

1. `auto` 会丢掉哪些限定？

   **答**：按值声明 `auto x = expr` 类似模板 `T` 按值推导，会移除引用和顶层 cv；底层 const 会保留。写 `auto&`/`const auto&` 时按引用规则推导，被引用对象的 cv 会保留。

2. `auto&&` 一定是右值引用吗？

   **答**：不一定。若 `auto` 发生推导，`auto&&` 是转发引用：绑定左值时折叠为左值引用，绑定右值时才是右值引用。已经确定类型别名后的 `Alias&&` 则通常只是普通右值引用。

3. `decltype(x)` 与 `decltype((x))` 为什么不同？

   **答**：无括号名字走特殊规则，直接返回声明类型；加括号后成为普通表达式，`x` 是左值表达式，所以得到 `T&`。这也是 `decltype(auto)` 返回引用时常需要括号的原因。

4. 数组传给 `template<class T> f(T)` 后 `T` 是什么？怎样保留长度？

   **答**：按值时数组退化为元素指针，`T` 是 `Element*`。使用 `template<class T, size_t N> f(T (&arr)[N])` 绑定数组引用，既不退化又能推导 `N`。

5. `std::decay_t<T>` 与 `std::remove_cvref_t<T>` 有何区别？

   **答**：两者都移除引用及顶层 cv；`decay` 还把数组变指针、函数变函数指针，模拟按值传参。`remove_cvref` 保留数组和函数本体类型。

6. 返回值什么时候适合用 `decltype(auto)`？

   **答**：泛型适配器需要精确保留被包装表达式的引用和值类别时适合，例如代理访问器。普通业务函数若不需要透传引用，明确类型或普通 `auto` 更安全，能降低意外悬垂风险。

7. 模板 `T&&` 什么时候不是转发引用？

   **答**：只有 `T` 是该函数调用中被推导的 cv-unqualified 模板参数时才是。类模板成员里的 `T&&`（T 已由类确定）、`const T&&`、显式指定 T 后的形参都不是转发引用。

8. 为什么函数形参 `T&& value` 在函数体里是左值？

   **答**：值类别属于表达式，不属于变量的声明类型。任何有名字的变量表达式都是左值，即使变量类型是右值引用；因此继续传递时要用 `std::forward<T>(value)` 恢复调用方的值类别。
