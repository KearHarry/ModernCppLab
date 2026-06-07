# A4 · 类型擦除与手写 std::function（type erasure）

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考
`std::function` 几乎人人会用，但"它怎么能装下函数指针、lambda、仿函数这些**完全不同的类型**？"能答上来的人不多。这背后是 **类型擦除（type erasure）**——C++ 里把"编译期千变万化的具体类型"收敛成"运行期统一接口"的核心手法，`std::any`、`std::shared_ptr` 的删除器、各种插件/回调系统全靠它。把这一题吃透，等于打通了"模板 + 虚函数"两套多态机制怎么配合。

## 背景知识

### 1) 矛盾：一个固定类型要装下无穷多种类型
`Function<int(int,int)>` 本身是**一个**确定的类型，可它要能持有：
```cpp
int add(int,int);                       // 函数指针类型
[](int a,int b){ return a*b; };         // 某个匿名 lambda 类型
[base](int a,int b){ return base+a+b; };// 另一个 lambda 类型（捕获不同→类型不同）
Multiplier{3};                          // 自定义仿函数类型
```
这些类型彼此毫无关系。我们**不能**把具体类型 `F` 写进 `Function` 的类型参数里（否则就不通用了）。于是必须把 `F` **擦掉**，对外只保留"能像 `int(int,int)` 那样被调用"这一抽象能力。

### 2) 类型擦除三件套：抽象接口 + 模板派生类 + 基类指针
```
┌─ 抽象接口 CallableBase ─┐   只规定能力，不含具体类型：
│ virtual invoke(...)     │     invoke / clone / 虚析构
│ virtual clone()         │
└─────────────────────────┘
          ▲ 继承
┌─ 模板派生类 CallableImpl<F> ─┐  把具体 F 藏在这里：
│ F f_;                        │    成员 f_ 保存真身
│ invoke(): return f_(args...) │    虚函数里调用它
│ clone():  new 一个同类型自己 │
└──────────────────────────────┘
          ▲ 被持有（向上转型为基类指针）
┌─ Function ─┐  对外只剩一根基类指针：
│ unique_ptr<CallableBase> callable_   ← F 被"擦"没了
└────────────┘
```
构造 `Function f = 某可调用对象` 时，`new CallableImpl<F>` 把 `F` 包进派生类，再用基类指针持有——**类型擦除就发生在这一步**。调用 `f(a,b)` 时经基类指针**虚调用** `invoke()`，多态分派回真正的 `f_`。这正是 D4 学的 vtable 动态分派，换到"擦除类型"的场景里用。

### 3) 为什么需要 clone()：可拷贝 + 已擦除
`std::function` 可拷贝。但 `Function` 内部只有一根基类指针，直接拷指针会两对象共享同一份目标（double free）；而类型已被擦除，普通拷贝构造"不知道要复制成哪个具体类型"。解法同 D4 的**虚拷贝**：基类声明 `virtual clone()`，每个 `CallableImpl<F>` 重写它 `make_unique` 出一个**同类型**的自己，于是 `callable_->clone()` 多态地深拷贝出正确类型——**原型模式**。

### 4) 完美转发构造的"自我劫持"坑
转换构造 `template<class F> Function(F f)` 太"贪婪"：当 `f` 本身是另一个 `Function` 时，它可能抢在拷贝/移动构造前面被选中，导致诡异行为。标准库的做法是用 `enable_if` 把"`F` 就是 `Function` 自己"的情况排除掉（本模块已在签名里写好这道护栏，你只需理解它为何存在）。

### 5) 和真正的 std::function 差在哪
真实 `std::function` 还有 **小对象优化（SBO）**：足够小的可调用对象直接塞进自身的内嵌缓冲区，省掉堆分配。本模块为聚焦"类型擦除"本身，统一走堆分配（`unique_ptr`），不做 SBO——理解了擦除，再加 SBO 只是工程优化。

## 你要实现什么
打开 [include/function.hpp](include/function.hpp)。抽象接口 `CallableBase`、拷贝/移动构造、`operator bool` 等脚手架已给好。你实现 4 个 `TODO`（签名都在，只填函数体）：

| 编号 | 位置 | 任务 |
|------|------|------|
| A4-1 | `CallableImpl<F>::invoke` | `return f_(args...);` 调用被擦除的目标 |
| A4-2 | `CallableImpl<F>::clone` | `make_unique<CallableImpl>(f_)` 虚拷贝出同类型自己 |
| A4-3 | `Function` 转换构造函数 | 把 `f` 包进 `CallableImpl<F>` 存入 `callable_`（擦除发生处）|
| A4-4 | `Function::operator()` | `return callable_->invoke(args...);` 经基类指针虚调用 |

## 关键坑
- **`clone()` 返回基类指针**：`make_unique<CallableImpl>(f_)` 自动向上转型为 `unique_ptr<CallableBase>`，别写成返回派生指针。
- **`invoke`/`operator()` 都是 `const`**：因此目标的 `operator()` 也要能在 const 下调用（普通 lambda、`const` 仿函数都满足；带 `mutable` 状态需另设计，本模块不涉及）。
- **转换构造的 enable_if 不要删**：它挡住"用一个 Function 构造另一个 Function 时走错重载"。
- **空 Function 不可调用**：未绑定目标就 `f()` 是未定义行为（和 `std::function` 一致）；调用前可用 `if (f)` 判空。
- **每个 lambda 类型都不同**：哪怕两个 lambda 长得一模一样，类型也不同——这正是必须擦除的原因。

## 如何验证
```powershell
ctest --test-dir build -R A4 --output-on-failure
```
测试覆盖：同一个 `Function` 类型分别装 **函数指针 / 无捕获 lambda / 有捕获 lambda / 仿函数** 并正确调用；**任意签名**（零参、返回 `string`）；**operator bool** 判空；**拷贝**经 `clone()` 深拷贝出独立可用的副本；**移动**转移所有权后源变空。

> 提示：骨架里转换构造为空、`operator()` 恒返回默认值、`clone` 返回 `nullptr`，所以"调用得到正确结果"的断言一开始全红；但 `operator()` 不解引用空指针、拷贝/移动对空对象有保护，全程不崩溃。clone 用例用 `ASSERT_TRUE(g)` 先挡住。

## 面试追问

1. `std::function` 怎么装下不同类型的可调用对象？类型擦除的本质是什么？

   **答**：内部持有一个指向**抽象基类**（带纯虚 `invoke(Args...)`）的指针，对每种具体可调用类型生成一个**模板派生类**包住它、重写 `invoke`；构造时 `new` 出对应派生对象、用基类指针存住——于是外层 `function<R(Args...)>` 类型统一，内部真实类型被"擦除"到只剩"能用这套签名调用"。类型擦除的本质：**用统一接口（虚表/函数指针）+ 堆上多态对象，把编译期的多种类型收敛成一个运行期类型**。

2. 类型擦除和模板有什么区别？为什么不能直接用模板参数存可调用对象？

   **答**：模板 `template<class F>` 会把 `F` 写进类型——`Holder<Lambda1>` 与 `Holder<Lambda2>` 是**不同类型**，没法放进同一 `vector`、没法当同一个类的同名成员、没法运行期切换。类型擦除把这些差异藏到运行期，对外只暴露一个 `function<R(Args)>`，于是能进同构容器、当成员、动态换。代价是堆分配 + 虚调用；模板零开销但类型会"传染"到外层。

3. `std::function` 拷贝时发生了什么？为什么需要 `clone()` 这样的机制？

   **答**：`function` 是值语义，拷贝它要**深拷贝**内部那个被擦除的对象。但基类指针不知道真实类型、无法直接 new 副本，所以抽象接口里加虚函数 `clone()`，由每个派生类返回 `new Derived(*this)`——多态地复制出正确类型。没有 clone 就只能浅拷指针，导致两个 function 共享同一对象、double free。

4. 调用 `std::function` 的开销来自哪里？SBO 能省掉哪一部分？

   **答**：①构造时一次**堆分配**（装派生对象）；②每次调用一次**虚函数间接跳转**；③跨类型边界**无法内联**。**SBO（小对象优化）**：在 function 内部留一小块缓冲，若可调用对象足够小（裸函数指针、无捕获 lambda）就**原地构造**、省掉堆分配那一部分；虚调用与不可内联依旧存在。

5. 空的 `std::function` 调用会怎样？怎么安全判空？

   **答**：调用空的 `function` 抛 `std::bad_function_call`。判空用它到 `bool` 的显式转换：`if (f) f();`。默认构造、用 `nullptr` 赋值、或被移动后都可能为空。

6. 为什么转换构造函数要用 `enable_if`/concept 约束？不加会出什么问题？

   **答**：约束"实参必须是能以 `R(Args...)` 调用的可调用对象"。不加约束，这个模板构造会**贪婪匹配一切类型**、污染重载决议——拷贝构造或用不兼容类型构造时本该报清晰错误，却被它拦截、产生晦涩的内部错误，甚至和拷贝构造打架。加 `enable_if_t<is_invocable_r_v<R,F,Args...>>` 让不合规的 F 直接 SFINAE 出局。

7. `std::function` vs 函数指针 vs 模板回调：各自的取舍？

   **答**：**函数指针**最轻（一个指针），但只能指无捕获的自由函数/无状态 lambda，存不了状态。**模板回调** `template<class F> void run(F)` 零开销、可内联、能接任何可调用体，但类型会进签名、不能跨编译单元、不能进同构容器、易代码膨胀。**`std::function`** 能统一存储有状态可调用体、可放容器可当成员，代价是堆分配 + 虚调用。按"要不要存状态 / 要不要统一类型 / 在不在热路径"权衡。

8. 只可移动的可调用对象为什么 `std::function` 存不了？`std::move_only_function` 解决了什么？

   **答**：`std::function` 要求内部对象**可拷贝**（它是可拷贝值类型，clone 依赖拷贝构造）。捕获了 `unique_ptr` 的 lambda 只能移动、不能拷贝，于是编译期就被拒。C++23 的 `std::move_only_function` 去掉"可拷贝"要求、只要求可移动，正好装这类 lambda，还能用 const/noexcept 限定更精确地表达调用约定。
