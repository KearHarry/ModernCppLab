# E6 · Sparse Set 与 ECS 组件存储

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考

游戏和实时仿真面试常从“实体很多、ID 很稀疏，怎样做到 O(1) 查找又能连续遍历”切入。Sparse set 用一个稀疏索引数组换来实体 ID 到 dense 槽位的 O(1) 映射，再把实际组件紧密排在连续数组里。它把哈希查询的随机访存与 vector 遍历的缓存友好结合，是 ECS、关系图和倒排集合的常用底座。

## 背景知识

### 1) sparse 与 dense 两侧

对于实体 `e`，`sparse[e]` 给出它在 `dense_entities` 中的候选下标 `i`；必须再验证 `dense_entities[i] == e`，才能确认实体仍存在。组件数组与实体 dense 数组严格平行：第 i 个实体的组件就在 `components[i]`。

### 2) O(1) 删除：swap-and-pop

删除 dense 中间元素会留下洞。把末元素移动到洞中，再 `pop_back`，即可避免 O(n) 搬移；别忘了把被搬实体的 `sparse` 索引改成新位置。代价是 dense 遍历顺序不稳定，指针/引用也可能失效。

### 3) 为什么适合 ECS

系统通常按组件类型批量扫描。`Position` 组件紧密存储时，硬件预取器能连续拉取 cache line；不存在 Position 的实体根本不会进入循环。与“每个 Entity 是一个装几十个成员的大对象”相比，这更贴合实际访问模式。

### 4) 与 Slot Map 的关系

E1 的 slot map 用 generation 防止旧句柄误命中新对象，强调稳定身份；sparse set 强调按组件稠密遍历。本模块直接接受 Entity ID，生产系统通常会把 E1 风格的 generation 校验放在实体管理器，再用 index 访问各组件池。

## 你要实现什么

打开 [include/sparse_set.hpp](include/sparse_set.hpp)：

| 编号 | 位置 | 任务 |
|---|---|---|
| E6-1 | `insert` | 插入或更新，维护 sparse/dense 映射 |
| E6-2 | `erase` | swap-and-pop 并修复被搬实体索引 |
| E6-3 | `each` | 连续遍历平行的实体与组件数组 |

## 关键坑

- 不能只看 `sparse[e]`，必须回查 dense 实体，防止旧索引假命中。
- 新实体 ID 可能很大，扩 sparse 时要用 `kSparseNpos` 初始化空槽。
- 插入已存在实体应更新而非重复追加。
- 删除末元素与删除中间元素要统一正确处理。
- swap-and-pop 会改变 dense 顺序，并使指向末元素/洞位置的引用失效。
- Entity ID 极端稀疏时，纯数组 sparse 会浪费内存，可分页或改哈希索引。

## 如何验证

```powershell
cmake --build build -j
ctest --test-dir build -R E6 --output-on-failure
```

测试覆盖插入更新、百万级稀疏 ID、删除后索引修复、dense 遍历、平行数组同步收缩，以及不可默认构造组件的搬移与精确析构计数。

## 面试追问

1. **Sparse set 为什么查询是 O(1)？**

   Entity 本身直接作为 sparse 数组下标，一次数组访问得到 dense 下标，再一次回查确认身份；没有哈希计算和冲突链。代价是 sparse 数组大小与最大 ID 相关。

2. **为什么必须做 `dense[sparse[e]] == e` 校验？**

   删除后 sparse 槽可能保留旧值，或默认值碰巧指向合法 dense 位置。回查实体身份才能区分“候选下标”与真实存在。

3. **swap-and-pop 的优缺点？**

   删除 O(1)、dense 保持无洞；但顺序不稳定，被搬元素地址改变，外部不能长期保存组件裸指针。需要稳定顺序时可留 tombstone 并批量整理，或使用间接层。

4. **Sparse set 与 unordered_map 怎么选？**

   ID 是紧凑整数且重视遍历时，sparse set 更快、更可预测；ID 范围巨大或键不是整数时，哈希表更节省索引空间。分页 sparse set 可折中。

5. **ECS 为什么常用 SoA？**

   一个系统往往只访问少数字段。SoA 只把所需字段连续载入缓存，便于 SIMD；AoS 会连同无关字段一起占 cache line。选择应由真实访问模式决定，不是 SoA 永远更快。

6. **怎样防止 Entity ID 重用导致悬垂引用？**

   把句柄拆为 index+generation，每次回收 index 时增加 generation，访问组件前核对代数。这正是 E1 slot map 的机制。

7. **怎样求两个组件集合的交集？**

   遍历较小集合的 dense 实体，对每个实体在另一集合做 O(1) `contains`；总复杂度 O(min(n,m))，且主遍历仍连续。

8. **组件池扩容会有什么影响？**

   底层 vector 重分配会使所有组件指针/引用失效。可提前 reserve、用 chunked storage，或只向外暴露 Entity/handle 而不长期暴露地址。
