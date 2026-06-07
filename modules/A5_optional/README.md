# A5 · 自己实现 Optional&lt;T&gt;（std::optional 的微缩版）

难度 ⭐⭐⭐ · 预计 2h

## 为什么大厂爱考
"函数可能没有返回值，你怎么表达？"——返回魔法值 `-1`、塞 `nullptr`、外带一个 `bool out` 参数，都既丑又易错。`std::optional<T>` 用一个"要么装着 T、要么为空"的盒子优雅解决。但真正考的是它的**实现**："空的时候那个 T 在哪？" 答案牵出 C++ 最核心的一项硬功夫——**在原地存储里手动管理对象生命周期**（union + placement new + 显式析构）。这套手法和 C2 的 SSO、C3 的内存池、`std::variant`、`std::any` 共用同一套底层，吃透 optional 等于拿到一把通用钥匙。

## 背景知识

### 1) 核心矛盾：空盒子里不能有"活着的 T"
最自然的写法是：
```cpp
T    value_;
bool engaged_;
```
但这样 `value_` **永远是个被构造好的对象**——即便逻辑上"空"，内存里仍住着一个真实的 T（于是 T 必须能默认构造，还白白占着资源），析构时机也完全不受你控制。这与 optional 的语义（空时**没有**任何 T 存活）相悖。

### 2) 解法：用 union 关掉"自动构造/析构"
```cpp
union { T value_; };   // union 成员不会被自动构造，也不会被自动析构
bool engaged_ = false; // engaged_==true 时，value_ 才是"活的"
```
union 成员的生命周期编译器**完全不管**，控制权 100% 交到你手里：
- **放值**：在 `value_` 这块内存上 **placement new** 一个 T → `::new (&value_) T(args...)`，置 `engaged_=true`。
- **清空**：显式调用析构函数 `value_.~T()`，置 `engaged_=false`。

"原地构造 + 显式析构"就是手动生命周期管理的全部精髓。

### 3) 特殊成员函数要"按状态"亲手搬运
因为 union 不替你管 `value_`，拷贝/移动/赋值/析构每一处都得你自己写，且**只在 `engaged_` 为真时**才碰 `value_`：
```
拷贝构造：对方有值 → 在自己存储上拷贝构造一个；对方空 → 自己也空。
移动构造：对方有值 → 在自己存储上移动构造一个（对方仍 engaged，与标准库一致）。
赋值：先 reset() 清掉自己，再按对方状态 emplace。
析构：有值 → 析构 value_（本模块统一走 reset()）。
```
漏了任何一处：要么**内存泄漏**（该析构没析构），要么**重复析构/读垃圾**（错误地碰了空盒子里的 `value_`）。

### 4) 两种访问：受检 vs 无检查
- `value()`：空则抛 `BadOptionalAccess`（受检，安全）。
- `operator*` / `operator->`：**无检查**，前置条件是"有值"，空盒子上调用是未定义行为——和 `std::optional` 完全一致（换取零开销）。
- `value_or(x)`：有值返回值的拷贝，否则返回你给的默认值。

## 你要实现什么
打开 [include/optional.hpp](include/optional.hpp)。默认/值构造、移动构造、拷贝/移动赋值、`value`/`operator*`/`value_or` 等都已给好。你实现 3 个 `TODO`（注释里附了近乎完整的伪代码）：

| 编号 | 函数 | 任务 |
|------|------|------|
| A5-1 | `reset()` | 有值则显式析构 `value_`、置空；空盒子调用是安全的空操作（生命周期的"收"）|
| A5-2 | `emplace(args…)` | 先 `reset()`，再 placement new 就地构造，置 `engaged_`，返回新值引用（生命周期的"放"）|
| A5-3 | `Optional(const Optional&)` | 对方有值则在自己存储上拷贝构造一个，否则保持空 |

> A5-3 也可直接写成 `if (other.engaged_) emplace(other.value_);`，复用 A5-2。

## 关键坑
- **placement new 要带 `static_cast<void*>`**：`::new (static_cast<void*>(&value_)) T(...)`，定位到那块内存原地构造，而不是分配新内存。
- **显式析构只在 `engaged_` 时做**：对空盒子调用 `value_.~T()` 是对未构造对象析构 → 未定义行为。`reset()` 里务必先判 `engaged_`。
- **emplace 必须先 reset**：否则旧值的析构被跳过 → 泄漏，且会在已有对象上 placement new → 覆盖且不析构。
- **union 成员不会自动析构**：所以析构函数必须手动 `reset()`，否则装着的 T 永远不被销毁。
- **移动后对方仍 engaged**：标准库 `std::optional` 移动**不**清空源（只是把里面的 T move 走），本模块与之一致——别在移动构造里顺手 `other.reset()`。
- **拷贝要深**：拷贝构造/拷贝赋值是在**自己的**存储上新构造一个 T，改副本不影响原对象。

## 如何验证
```powershell
ctest --test-dir build -R A5 --output-on-failure
```
测试覆盖：默认**空**、`emplace` 后**有值且可解引用**（返回引用可写）；**值构造/隐式构造/拷贝/移动**四种建立方式；拷贝是**深拷贝**（改副本不动原件）、拷空仍空；**拷贝赋值/移动赋值/用空覆盖**；`reset` 清空且**重复 reset 安全**；`value()` 空时**抛 `BadOptionalAccess`**、`value_or` 回退默认；**生命周期**用 `Tracked` 计数验证"恰构造一次、恰析构一次、作用域结束不泄漏不重复析构"。

> 提示：骨架里 `reset` 为空操作、`emplace` 不真正构造、拷贝构造为空体，于是 Optional 永远"空"。访问内容前都用 `ASSERT_TRUE(has_value())` 挡住，所以一开始只变红、不崩溃。

## 面试追问

1. `std::optional<T>` 空的时候，那个 `T` 占内存吗？为什么不能简单地用 `T value_; bool has_;`？

   **答**：占——`optional` 大小至少是 `sizeof(T)` 加一个 bool（再加对齐填充），空间始终为 T 预留，只是**没构造对象**。不能用 `T value_; bool has_;` 因为那会**无条件构造** `T`：T 可能没有默认构造、或默认构造有副作用/开销；而 optional 的语义是"空时那个 T 根本不存在"。所以要用 union + placement new 手动掌控 T 的生命周期。

2. 为什么用 `union` 而不是 `alignas(T) unsigned char buf[sizeof(T)]`？两者都行，区别在哪？

   **答**：两者都能"留原始内存、手动构造"。区别：`union { T value_; }` 让编译器**自动算好大小与对齐**，且对**平凡类型**能自动平凡复制/平凡析构（它知道里面是个 T）；裸字节数组要自己 `alignas`、自己 `reinterpret_cast`，丢失平凡性优化、更易踩别名（aliasing）问题。union 更声明式、更安全。

3. placement new 构造的对象，为什么必须手动调析构函数？什么时候可以省略？

   **答**：placement new 只在你给的内存上**构造**对象、不管理内存，编译器不会自动帮你析构（它不知道 union 此刻"是否有活对象"）。所以销毁/重置前必须显式 `value_.~T()` 运行析构、释放 T 持有的资源。当 T 是**平凡可析构**（trivially destructible，如 int）时析构是空操作，可省略——但写上也无害。

4. `optional` 的移动会清空源吗？为什么标准库选择"移动后源仍有值"？

   **答**：不会清空 engaged 状态。`optional` 的移动是**移动里面的 T**：源若有值，移动后源**依然 `has_value()==true`**，只是里面的 T 处于被移动状态（valid but unspecified）。这样设计是为语义一致——"有没有值"和"值是什么"是两回事，移动只动"值"、不动"在不在"。想让源变空要显式 `reset()`。

5. `value()` 和 `operator*` 有什么区别？为什么标准库提供两种访问、各自的代价是什么？

   **答**：`value()` 在空时**抛 `std::bad_optional_access`**，是带检查的安全访问；`operator*`（和 `operator->`）**不检查**，空时是未定义行为，但因此零开销。规则：不确定有没有值、或在边界处用 `value()`；已判过 `has_value()` 的热路径用 `*` 省掉重复检查。

6. `optional<T&>` 为什么标准库不支持（直到 C++26 才讨论）？引用放进 union 有什么麻烦？

   **答**：引用不能被重新绑定、不能放进 union（引用不是对象、没有存储），语义也含糊：`opt = x` 究竟是"重绑引用"还是"给被引用对象赋值"？赋值语义争议加实现麻烦，使标准长期没纳入。想要"可空的引用"现在一般用 `T*` 或 `reference_wrapper`。

7. 异常安全：`emplace` 里 placement new 抛异常了，盒子应处于什么状态？

   **答**：应为**空**。`emplace` 一般先 `reset()` 清旧值、再 placement new 构造新值；若构造中途抛异常，新对象没建立成功，就**不能**把 engaged 置真——否则会留下"标记为有值、实则半个对象"的盒子，之后析构它就是 UB。正确实现：构造成功后才置 `has_=true`，异常路径上 has_ 保持 false。

8. `optional`、`variant`、`any`、`expected` 在"原地存储 + 手动生命周期"上有什么共性？它们各自解决什么问题？

   **答**：它们都用**对齐原始存储 + placement new + 显式析构**就地管理"可能存在"的对象、避免堆分配。区别在装什么：`optional<T>`=0或1个 T；`variant<Ts...>`=恰好一个、类型在编译期集合里（带类型标签）；`any`=任意类型（类型擦除、常需堆分配）；`expected<T,E>`=要么值 T 要么错误 E，用于不抛异常的错误处理。共性是"可空/和类型"的值语义容器。
