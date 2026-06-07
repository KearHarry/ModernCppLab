# A3 · 模板元编程 (Template Metaprogramming)

难度 ⭐⭐⭐ · 预计 2h

## 为什么大厂爱考
模板元编程是「在编译期用类型做计算」的能力，标准库（`type_traits`、`vector`、`tuple`…）全靠它。面试常问：`std::move`/`std::forward` 怎么实现、SFINAE 是什么、`enable_if`/`void_t` 怎么用、C++20 concepts 比 SFINAE 好在哪。手写几个 trait，你就理解了 `<type_traits>` 的全部魔法。

## 背景知识

### 1) 模板特化：编译期的「if / 模式匹配」
主模板 (primary template) 是「默认情况」，**特化 (specialization)** 针对特定类型给出不同实现。编译器会优先选最匹配的特化。这就是元编程做「分支」的方式：
```cpp
template <class T> struct IsPointer            { static constexpr bool value = false; };
template <class T> struct IsPointer<T*>        { static constexpr bool value = true;  };
//                              ^^^ 偏特化：当 T 是"某类型的指针"时命中
```

### 2) 类型萃取 (type traits)
trait 就是「输入类型 → 输出类型或常量」的编译期函数。约定俗成：
- 输出**类型**放在 `::type`（常配一个 `XxxT` 别名简化）。
- 输出**布尔/数值**放在 `::value`（常配一个 `XxxV` 变量模板）。
标准库的 `std::remove_reference<T>::type`、`std::is_same<A,B>::value` 都是这个套路。

### 3) SFINAE 与 `void_t` 检测惯用法
**SFINAE** = Substitution Failure Is Not An Error：模板参数替换失败时，不报错，只是把这个候选「悄悄丢弃」。它让我们能根据「类型是否支持某操作」来选择重载或特化。

现代最常用的写法是 **`std::void_t` 检测惯用法**——探测某类型有没有某成员：
```cpp
template <class T, class = void> struct HasSize : std::false_type {};   // 默认：没有
template <class T> struct HasSize<T, std::void_t<decltype(std::declval<T&>().size())>>
                                 : std::true_type {};                   // 有 .size() 才命中
```
若 `T` 没有 `.size()`，`decltype(...)` 替换失败 → 偏特化被丢弃 → 落到主模板 `false`。这正是 SFINAE。

### 4) 可变参数模板与折叠表达式 (C++17)
`template <class... Ts>` 接收任意个参数；`sizeof...(Ts)` 取个数。**折叠表达式**把一个二元运算「折」到整个参数包上：
```cpp
(xs + ...)          // 一元右折叠： x0 + (x1 + (x2 + ...))
(0 + ... + xs)      // 二元左折叠：((0 + x0) + x1) + ...   ← 能处理空包
(... && flags)      // 全部为真？
```
C++17 之前要靠递归模板才能做到，折叠表达式让代码短得多。

### 5) C++20 Concepts：给模板参数加「约束」
concept 是「对类型的要求」，本质是个编译期 `bool`，但能直接约束模板、改善报错、参与重载决议：
```cpp
template <class T> concept Addable = requires(T a) { a + a; };  // 能做 a+a 就满足
template <Addable T> T twice(T x) { return x + x; }            // 只接受 Addable 类型
```
相比 SFINAE/`enable_if`，concepts **可读性更好、报错更清晰、写法更直接**，是面试新热点。

## 你要实现什么
打开 [include/meta.hpp](include/meta.hpp)，共 8 个 `TODO`：

| 编号 | 任务 | 知识点 |
|------|------|--------|
| A3-1 | `RemoveReference` 的两个特化 | 偏特化剥离引用（`std::move` 的基础） |
| A3-2 | `IsSame` 的相等特化 | 偏特化做类型比较 |
| A3-3 | `Conditional` 的 false 特化 | 编译期三元选择 |
| A3-4 | `HasSize` 的 `void_t` 特化 | SFINAE 成员检测惯用法 |
| A3-5 | `sum(...)` 折叠表达式 | 可变参 + 折叠 |
| A3-6 | `all_of(...)` 折叠表达式 | 逻辑折叠 |
| A3-7 | `count(...)` | `sizeof...` |
| A3-8 | `Addable` 概念 | C++20 concept + `requires` |

## 关键坑
- **偏特化要写在主模板之后**；`template <>` 是全特化，`template <class T> X<T*>` 是偏特化。
- **`::type` 前面常要加 `typename`**（依赖名消歧义），别名模板 `XxxT` 帮你省掉它。
- **`std::declval<T>()` 只能用在不求值语境**（`decltype`/`sizeof` 内），它「假装」造一个 T 用来探测，不真的构造。
- **一元折叠不能处理空参数包**（如 `(xs + ...)` 在 0 个参数时报错），需要默认值就用二元折叠 `(0 + ... + xs)`。
- **concept 永远不会「硬报错」**：不满足只是求值为 `false`，所以可以放心 `EXPECT_FALSE(Addable<X>)`。

## 如何验证
```powershell
ctest --test-dir build -R A3 --output-on-failure
```
测试把 trait 的 `::value` / `::type` 用运行期 `EXPECT_*` 检查（不用 `static_assert`，这样骨架阶段也能编过、看到「红」），覆盖引用剥离、类型相等、条件选择、成员检测、折叠求和/逻辑、参数计数、概念判定。

## 面试追问

1. `std::move` 和 `std::remove_reference` 的源码大概长什么样？

   **答**：`remove_reference` 是个 trait：主模板 `template<class T> struct remove_reference { using type = T; };`，加两个偏特化 `<T&>`、`<T&&>` 都把 `type` 定为 `T`，从而剥掉引用。`std::move` 借它实现：`template<class T> constexpr remove_reference_t<T>&& move(T&& x) noexcept { return static_cast<remove_reference_t<T>&&>(x); }`——先去引用、再强转成右值引用。

2. SFINAE 一句话解释；`enable_if` 和 `void_t` 各自怎么用？

   **答**：SFINAE =「替换失败不是错误」：把模板实参替换进签名时若产生非法类型，该候选被**静默剔除**出重载集，而非编译报错。`enable_if<cond,T>` 仅当 `cond` 为真才有 `::type`，放在返回类型或模板默认参数里、用条件控制某重载是否参与。`void_t<...>` 把"一组类型表达式合法"映射成 `void`，配合偏特化做**成员/能力检测**（表达式非法→偏特化失配→回落主模板的 false）。

3. `std::true_type` / `std::false_type` 是什么？为什么 trait 喜欢继承它们？

   **答**：它们是 `integral_constant<bool,true/false>` 的别名，内含 `static constexpr bool value` 与到 `bool` 的转换。trait 继承它们就**白拿** `::value`、隐式转 bool、`()` 调用等整套接口，无需重复定义，还能和标准库元设施（如 `conjunction`）无缝配合。写 `struct is_foo : true_type {}` 比手写 `value=true` 更统一规范。

4. 折叠表达式四种形式（左/右 × 一元/二元）分别展开成什么？

   **答**：设包 `args...`、运算符 `op`：**一元右折叠** `(args op ...)`→`a1 op (a2 op (… op aN))`；**一元左折叠** `(... op args)`→`((a1 op a2) op …) op aN`；**二元右折叠** `(args op ... op init)`→`a1 op (… op (aN op init))`；**二元左折叠** `(init op ... op args)`→`((init op a1) op …) op aN`。带 init 的二元形式能给**空包**提供初值、避免空包对某些运算符非法。

5. concepts 相比 SFINAE 有哪些好处？`requires` 表达式和 `requires` 子句的区别？

   **答**：concepts 的约束**可读、可命名、可复用**，报错直指"哪个约束没满足"，而 SFINAE 失败往往是一坨晦涩的重载推导错误；concepts 还参与重载偏序（更具体的约束优先）。**`requires` 子句**是加在模板上的布尔约束（`template<class T> requires C<T>`）；**`requires` 表达式** `requires(T a){ … }` 是用来检验"一组操作是否合法"的求值式，常被用来**定义** concept。二者常连用：`requires requires(...){...}`。

6. 模板的「两阶段名字查找」是什么？为什么有时要写 `typename` / `template` 关键字？

   **答**：模板在**定义时**（第一阶段）就解析所有与模板参数无关的名字；与参数相关的"待决名字"（dependent names）留到**实例化时**（第二阶段）再查。定义阶段编译器不知道 `T::X` 是类型还是值，默认当**值**，所以要写 `typename T::X` 明确"这是类型"；同理 `obj.template f<int>()` 里的 `template` 告诉它 `f` 是成员模板、`<` 是模板实参而非小于号。
