# A10 · Lambda、捕获与 `std::invoke`

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考

Lambda 是现代 C++ 回调、并发任务和 ranges 的日常工具，但面试不会停在语法：值捕获何时复制、`mutable` 改的是谁、引用捕获为什么悬垂、泛型 lambda 如何实例化，以及成员函数指针为什么不能像普通函数指针那样直接调用。`std::invoke` 正是标准库统一这些调用形式的底座。

本模块实现三种捕获型 lambda，以及保留具体类型、可记录调用次数的 `InvokeBox`。它与 A4 的 `Function` 不重复：A4 研究类型擦除和虚分派，A10 研究保留 F 类型的泛型调用与 `std::invoke`。

## 背景知识

### 1) Lambda 是匿名闭包类型

每个 lambda 表达式都会生成唯一、不可命名的闭包类；捕获项通常成为它的数据成员，函数体成为 `operator()`。即使两个 lambda 文本完全相同，它们也是不同类型。`auto` 或模板参数能接住该类型。

### 2) 值捕获与引用捕获

`[x]` 把当时的 x 复制进闭包，原变量后来变化不影响副本；`[&x]` 保存对原对象的引用，能观察和修改它，但闭包绝不能活得比 x 久。`[this]` 捕获的是 this 指针，也有同样生命周期风险；C++17 的 `[*this]` 会复制整个对象。

### 3) `mutable` 修改闭包副本

lambda 的 `operator()` 默认是 const，所以值捕获成员默认不能修改。加 `mutable` 后可以改变闭包内部副本，不会让外部原变量变成可修改。复制一个有状态 lambda 会复制当时的闭包状态，两个副本随后各自演进。

### 4) 泛型 lambda 是带模板调用运算符的闭包

`[](auto x) { return x + x; }` 近似一个含 `template<class T> operator()(T)` 的匿名类。每种参数类型会实例化一份调用运算符；C++20 还能写显式模板参数列表 `[]<class T>(T x)` 并添加 concepts。

### 5) 成员指针不是普通地址

`&Widget::read` 需要同时提供对象才能调用，语法可能是 `(obj.*pmf)()` 或 `(ptr->*pmf)()`；数据成员指针也需要对象求值。多重继承下它甚至可能携带 this 调整信息，大小未必等于 `void*`。

### 6) `std::invoke` 统一可调用协议

`std::invoke(f,args...)` 能处理普通函数、函数对象、lambda、成员函数指针、数据成员指针，并识别对象、指针和 `reference_wrapper`。`std::is_invocable`、`invoke_result_t`、`std::apply` 等设施都建立在同一调用规则上。

## 你要实现什么

打开 [include/lambda_invoke.hpp](include/lambda_invoke.hpp)，完成 5 组 `TODO`：

| 编号 | 任务 | 知识点 |
|------|------|--------|
| A10-1 | `make_counter` | 值捕获、`mutable`、闭包独立状态 |
| A10-2 | `make_reference_updater` | 引用捕获与生命周期契约 |
| A10-3 | `make_scaler` | 泛型 lambda 与返回类型推导 |
| A10-4 | `invoke_member` | 成员函数/数据成员指针、完美转发 |
| A10-5 | `InvokeBox::operator()` | `std::invoke`、`invoke_result_t`、调用计数 |

## 关键坑

- 不要从函数返回捕获其局部变量引用的 lambda；函数返回后引用立即悬垂。
- `[=]` 并不自动让异步任务安全：隐式捕获的 `this` 仍只是指针。
- `mutable` 只去掉闭包调用运算符的 const，不会改变按引用捕获对象本身的 const 性。
- 完美转发必须使用 `std::forward<Args>(args)...`，否则所有有名字形参都会变成左值。
- `std::function` 要求目标可拷贝且可能分配；模板化 InvokeBox 可接 move-only F、便于内联，但类型会向外传播。
- 数据成员指针经 `std::invoke` 可返回真实引用，包装器不要无意间用普通 `auto` 把它复制成值。
- 本实验的 int 计数器/累加器以“结果仍在 int 范围内”为前置条件；通用库若接收不受信输入，应改用检查算术或明确的饱和/回绕策略，不能触发有符号溢出。

## 如何验证

```powershell
cmake --build build -j
ctest --test-dir build -R A10 --output-on-failure
```

测试覆盖可变值捕获及复制、引用捕获、泛型 lambda 多类型实例化，成员函数指针对对象/指针/`reference_wrapper` 的调用、数据成员引用，以及普通函数、lambda、仿函数、void/引用返回的统一包装。

## 面试追问

1. Lambda 的本质是什么？

   **答**：编译器生成的匿名闭包类对象，捕获通常成为成员，函数体成为 `operator()`。无捕获 lambda 还可转换成兼容的普通函数指针；每个 lambda 表达式都有独立类型。

2. 值捕获加 `mutable` 会修改外部变量吗？

   **答**：不会。值捕获在闭包里保存副本，mutable 只是允许非 const 地修改该副本。若要修改外部对象需引用捕获，并保证生命周期和线程同步。

3. `[this]` 与 `[*this]` 有什么区别？

   **答**：`[this]` 复制 this 指针，闭包仍访问原对象，对象先析构就悬垂；`[*this]` 复制整个对象进闭包，闭包访问自己的副本，但要求对象可复制且复制成本可能较高。

4. 泛型 lambda 与函数模板有什么关系？

   **答**：泛型 lambda 的闭包类拥有模板化 `operator()`，推导与实例化规则类似函数模板。它同时还是一个可携带捕获状态的对象，因此可传给算法、保存或组合。

5. `std::invoke` 解决了什么问题？

   **答**：把普通 callable 与成员指针的多套调用语法统一为一个接口，并支持对象、对象指针和 `reference_wrapper`。泛型库无需自己分支判断 `f(args...)`、`obj.*pm` 或 `ptr->*pm`。

6. 成员函数指针能转成 `void*` 吗？大小一定是一个指针吗？

   **答**：不能可移植地当普通地址使用，大小也不保证等于数据指针。多重/虚继承 ABI 下它可能包含函数标识和 this 调整信息，必须通过语言的成员指针语法或 `std::invoke` 使用。

7. 为什么转发包装器要用 `decltype(auto)` 或 `invoke_result_t`？

   **答**：被包装函数可能返回引用或 void；普通 `auto` 会丢引用。精确返回类型能保留原 callable 的接口语义，配合 `std::forward` 才称得上透明包装。

8. Lambda 捕获引用放进异步任务有什么风险？

   **答**：任务可能在原作用域退出后才运行，引用和 this 指针会悬垂；多线程访问同一对象还可能数据竞争。通常应按值捕获拥有所有权的数据（如 shared_ptr）或建立明确的 join/cancel 生命周期边界。
