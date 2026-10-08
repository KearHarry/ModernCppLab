# C10 · 迭代器、算法与 Ranges

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考

会背 `vector` 和 `list` 的底层结构，只掌握了 STL 的“容器”一半。STL 真正强大的地方是：算法不认识具体容器，只依赖迭代器所承诺的操作；C++20 ranges 又用 concept、sentinel 和 projection 把这份契约显式化。迭代器类别、半开区间、`lower_bound` 前置条件、失效规则和 `auto` 推导陷阱都是高频追问。

## 背景知识

### 1) 迭代器是一份能力契约

input 迭代器支持单遍读取；forward 支持多遍；bidirectional 增加 `--`；random access 增加常数时间的 `+/-/[]`；contiguous 还保证物理连续。算法按最弱够用的契约编写，因此同一个 `find` 可以服务数组、vector 和链表。

### 2) 为什么统一用 `[first, last)`

半开区间的长度是 `last-first`，空区间自然是 `first==last`，相邻区间 `[a,b)` 与 `[b,c)` 无重叠也无缝拼接。`last` 指向尾后位置，只能比较，不能解引用。

### 3) `iterator_category` 与 `iterator_concept`

旧算法通过 tag dispatch 读取 `iterator_category`；C++20 ranges 优先用 `iterator_concept` 和 concept 检查表达式语义。自定义迭代器若声称 random access，就必须保证差值、位移和下标是常数时间且彼此一致。

### 4) Projection

传统算法经常需要写“比较对象的某字段”的比较器；ranges 把“先取字段”单独抽为 projection，例如按 `Record::id` 排序或二分。这样比较关系仍作用在投影后的普通值上，组合更自然。

## 你要实现什么

打开 [include/iterators_ranges.hpp](include/iterators_ranges.hpp)：

| 编号 | 位置 | 任务 |
|---|---|---|
| C10-1 | `StridedIterator` 的 ++/-- | 前后移动逻辑下标 |
| C10-2 | 随机访问操作 | 基于逻辑下标实现 `+=`、`-=`、`[]` 与差值 |
| C10-3 | `StridedView::end` | 用逻辑下标生成尾后迭代器，不越界构造物理指针 |
| C10-4 | `projected_lower_bound` | 用 `std::invoke` 调用 projection 并实现 O(log n) 二分 |

## 关键坑

- 后置 `it++` 返回旧值，前置 `++it` 返回更新后的自身引用。
- 迭代器保存 base、stride 与逻辑下标；差值直接是逻辑下标之差。这样即使最后一个
  元素恰好位于底层数组末端，也不会为了表示 `end()` 而构造越过 one-past 的指针。
- `end()` 永远不能解引用；空 view 的 begin 与 end 必须相等。
- 本实验只接受正 stride；零 stride 无法形成有效范围，负 stride 还会反转随机访问排序。
- 只可比较来自同一序列、同一 stride 的迭代器；跨容器比较不具语义。
- `lower_bound` 要求按同一投影与顺序预先分区/排序，否则结果没有保证。
- `std::advance` 对链表是 O(n)，对随机访问迭代器才是 O(1)。

## 如何验证

```powershell
cmake --build build -j
ctest --test-dir build -R C10 --output-on-failure
```

测试覆盖跨步遍历、随机访问、标准 ranges 排序、const 迭代器、非正 stride 拒绝、lambda/成员指针 projection 二分与边界条件；编译期还会用 concept 检查迭代器契约。

## 面试追问

1. **五类传统迭代器的能力如何递增？**

   input/output 是单遍读或写；forward 可多遍且可默认构造；bidirectional 增加后退；random access 增加常数时间跳转、差值和下标；C++17 又明确 contiguous，额外保证地址连续。

2. **为什么 `list` 不能传给要求 random access iterator 的算法？**

   链表无法在 O(1) 时间完成 `it+n` 或两个位置求距离。若假装满足契约，二分和排序等算法的复杂度承诺就会被破坏。

3. **iterator、pointer、reference 何时失效？**

   取决于容器和操作：vector 重分配全部失效，无重分配的插入/删除通常使操作点之后失效；list 删除仅使被删节点失效；unordered 容器 rehash 使迭代器失效但标准节点引用通常仍有效。必须逐容器记契约。

4. **`std::lower_bound` 返回什么？**

   返回第一个使 `element < value` 为假的位置，也就是第一个“不小于 value”的元素；若都小于则返回 last。它不是“找到等于值才返回”。

5. **ranges 的 sentinel 为什么可以和 iterator 是不同类型？**

   有些流或 C 字符串的终点不是同类位置，而是“遇到 EOF/零字符”的条件。异构 sentinel 能更直接表达终止，避免人为构造一个昂贵或不存在的尾迭代器。

6. **projection 与 comparator 有什么区别？**

   projection 先把元素映射成比较键，comparator 决定两个键的顺序。`ranges::sort(rows, less{}, &Record::id)` 因而可以复用标准 less，不必写二参数字段比较 lambda。

7. **为什么 `std::vector<bool>::iterator` 常让泛型代码意外？**

   解引用返回位代理而不是真正的 `bool&`，所以取地址、引用绑定和 `auto` 保存结果可能与普通容器不同。泛型代码不应无条件假设 iterator 的 reference 就是 `value_type&`。

8. **迭代器 concept 通过就一定语义正确吗？**

   不一定。concept 主要检查表达式和部分类型关系，无法证明 `++` 真向前、差值一致或复杂度为 O(1)。这些仍是实现者必须遵守并用测试审查的语义契约。
