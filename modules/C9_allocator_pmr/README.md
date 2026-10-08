# C9 · 标准分配器与 PMR（Allocator / `allocator_traits` / `memory_resource`）

难度 ⭐⭐⭐⭐ · 预计 3h

## 为什么大厂爱考

“内存池怎么接进 STL？”往往比“怎么从 free-list 拿一块内存”更能区分工程能力。标准容器不直接依赖某个池，而是通过 Allocator 协议把分配策略参数化；C++17 的 PMR 又把这份策略从编译期模板参数变成运行时的 `memory_resource*`。这会牵出对象生命周期、节点 rebind、有状态分配器、对齐、容器移动/交换规则和异常安全。

## 背景知识

### 1) 分配与构造是两件事

`allocate(n)` 只取得足以容纳 `n` 个 `T` 的生内存，不构造对象；容器通过 `allocator_traits::construct`（C++20 后通常落到 `construct_at`）构造，通过 `destroy` 析构，再调用 `deallocate` 还内存。它与 C1 手写 vector 的四阶段生命周期完全对应。

### 2) 为什么需要 `allocator_traits`

容器不应假设分配器恰好拥有所有嵌套类型。`allocator_traits<A>` 补齐默认类型、统一 `allocate/construct/destroy` 调用，并通过 rebind 把 `Allocator<T>` 改绑到内部节点类型。`list<T, A>` 真正分配的是 `Node<T>`，这就是本模块用共享统计状态验证 rebind 的原因。

### 3) 有状态分配器的相等性

两个 allocator 相等，含义不是“字段碰巧相同”，而是：由一方分配的内存可以交给另一方释放。不同内存池通常不相等。`propagate_on_container_*` 与 `is_always_equal` 会影响容器移动赋值和 `swap` 能否只偷指针。

### 4) PMR：运行时选择资源

`std::pmr::polymorphic_allocator<T>` 内部只保存 `memory_resource*`；`memory_resource` 用三个虚函数处理分配、释放与资源等价性。业务容器类型不必随内存策略改变，适合按帧 arena、请求级资源和可插拔统计器。

## 你要实现什么

打开 [include/allocator_pmr.hpp](include/allocator_pmr.hpp)，完成 4 个 TODO：

| 编号 | 位置 | 任务 |
|---|---|---|
| C9-1 | `CountingAllocator::allocate` | 成功分配后记录次数、累计/在途字节 |
| C9-2 | `CountingAllocator::deallocate` | 对称释放并扣减在途字节 |
| C9-3 | `CountingResource::do_allocate` | 保留 alignment 转发并记账 |
| C9-4 | `CountingResource::do_deallocate` | 用原参数释放并记账 |

骨架会真实地向标准上游申请和归还内存，所以始终安全可编译；它只是故意不记账，测试会稳定变红。

## 关键坑

- `n` 是元素个数，统计字节应为 `n * sizeof(T)`，并注意溢出由上游分配器处理。
- 先分配、成功后再修改统计；否则 `bad_alloc` 会制造“幽灵内存”。
- `deallocate` 必须收到与分配匹配的数量/字节数和 alignment。
- 不要丢掉过对齐请求；`alignas(128)` 类型不能只按 `max_align_t` 对齐。
- allocator 相等关系决定跨容器释放是否合法，不是普通的值比较。
- PMR 资源必须比使用它的全部容器活得更久；容器只持裸指针，不拥有资源。

## 如何验证

```powershell
cmake --build build -j
ctest --test-dir build -R C9 --output-on-failure
```

测试覆盖 vector 扩容记账、list 节点 rebind、资源身份、128 字节过对齐、不同 PMR 资源以及作用域结束后在途字节归零。

## 面试追问

1. **Allocator 的 `allocate` 会构造对象吗？**

   不会。它只提供生内存；构造和析构由容器分别调用 `construct_at`/`destroy_at` 完成。这样 `capacity` 中未使用的槽位可以保持“没有对象”的状态。

2. **`allocator_traits` 解决了什么？**

   它是容器与自定义 allocator 的适配层：补默认类型、统一调用、完成节点 rebind，并读取传播 traits。容器因此不必要求每个 allocator 都重复一整套样板。

3. **为什么 `list<T>` 会把 allocator rebind 到别的类型？**

   list 分配的不是裸 `T`，而是含前后指针和 `T` 的内部节点。它必须把用户给的 `Allocator<T>` 改绑为节点 allocator，同时保留同一资源状态。

4. **两个 allocator 何时算相等？**

   当一方分配的内存可由另一方安全释放时。无状态的全局 new/delete allocator 通常总相等；指向不同 arena 的 allocator 通常不等。

5. **PMR 比模板 allocator 好在哪里？代价是什么？**

   同一个 `pmr::vector<T>` 类型可在运行时接不同资源，减少模板类型扩散并便于配置；代价是一层虚调用和资源生命周期由使用者保证。多数批量分配场景中，这点调度成本远小于实际分配成本。

6. **`monotonic_buffer_resource` 为什么释放单个对象没效果？**

   它采用 bump 分配，单次 `deallocate` 通常是空操作，最后整体 `release`。因此极快但只适合生命周期成组结束的对象。

7. **移动一个 allocator 不相等的 vector 一定是 O(1) 吗？**

   不一定。若 allocator 不传播且目标/源 allocator 不等，目标不能接管由另一资源分配的缓冲，往往要逐元素移动到自己的资源，复杂度变成 O(n)。

8. **怎么给 allocator 做异常安全统计？**

   先让上游完成可能抛出的分配，拿到指针后再提交统计；释放本身按协议不抛。更复杂的并发统计还需用原子或锁，且不能让记账抛异常导致已分配内存泄漏。

