# 面试考点速查索引

> 面试前快速复盘用：从「常被问到的问题」反查到「对应模块」。把每个问题当口试题，能脱口而出讲清楚才算过关。

## 现代 C++ 特性

| 高频问题 | 模块 |
|----------|------|
| 什么是右值引用？`std::move` 真的「移动」了吗？ | [A1](../modules/A1_move_semantics) |
| `std::move` 和 `std::forward` 区别？什么是完美转发？ | [A1](../modules/A1_move_semantics) |
| 移动构造为什么要写 `noexcept`？不写会怎样？ | [A1](../modules/A1_move_semantics) / [C1](../modules/C1_vector) |
| Rule of Three / Five / Zero 是什么？ | [A1](../modules/A1_move_semantics) |
| `unique_ptr` 怎么实现？为什么不能拷贝？ | [A2](../modules/A2_smart_pointers) |
| `shared_ptr` 的控制块里有什么？引用计数为什么要原子？ | [A2](../modules/A2_smart_pointers) |
| `weak_ptr` 解决什么问题？循环引用怎么破？ | [A2](../modules/A2_smart_pointers) |
| `make_shared` 相比 `shared_ptr(new T)` 的优势？ | [A2](../modules/A2_smart_pointers) |
| `enable_shared_from_this` 为什么存在？ | [A2](../modules/A2_smart_pointers) |
| 什么是 SFINAE？`enable_if` 怎么用？ | [A3](../modules/A3_template_metaprogramming) |
| 可变参模板/折叠表达式怎么写？ | [A3](../modules/A3_template_metaprogramming) |
| C++20 concepts 相比 SFINAE 好在哪？ | [A3](../modules/A3_template_metaprogramming) |

## 并发编程

| 高频问题 | 模块 |
|----------|------|
| `lock_guard` 和 `unique_lock` 区别？ | [B2](../modules/B2_blocking_queue) |
| 条件变量为什么必须配 `while` 谓词？什么是虚假唤醒？ | [B2](../modules/B2_blocking_queue) |
| 生产者-消费者怎么实现？怎么优雅关闭？ | [B2](../modules/B2_blocking_queue) |
| `atomic` 和加锁的区别？什么是 CAS？ | [B3](../modules/B3_atomics_memory_order) |
| `memory_order` 有哪些？acquire/release 是什么语义？ | [B3](../modules/B3_atomics_memory_order) |
| 手写一个自旋锁。它和 mutex 各自适用什么场景？ | [B3](../modules/B3_atomics_memory_order) |
| 手写线程池。任务怎么返回结果？ | [B5](../modules/B5_thread_pool) |
| `future`/`promise`/`packaged_task`/`async` 关系？ | [B5](../modules/B5_thread_pool) |
| 线程池怎么优雅停机？析构时未完成任务怎么办？ | [B5](../modules/B5_thread_pool) |

## STL 底层

| 高频问题 | 模块 |
|----------|------|
| `vector` 扩容是几倍？为什么不是 2 倍而常是 1.5 倍？ | [C1](../modules/C1_vector) |
| `size` vs `capacity`？`reserve` 和 `resize` 区别？ | [C1](../modules/C1_vector) |
| `push_back` 的均摊复杂度为什么是 O(1)？ | [C1](../modules/C1_vector) |
| 扩容时元素是拷贝还是移动？和 `noexcept` 什么关系？ | [C1](../modules/C1_vector) |
| 什么是 SSO（小字符串优化）？为什么 `sizeof(std::string)` 是 32？ | [C2](../modules/C2_string_sso) |
| `unordered_map` 底层结构？负载因子和 rehash？ | [C4](../modules/C4_hash_table) |
| `map` vs `unordered_map`：何时用哪个？ | [C4](../modules/C4_hash_table) |
| 哪些操作会让迭代器失效？ | [C1](../modules/C1_vector) / [C4](../modules/C4_hash_table) |

## 经典工程题与对象模型

| 高频问题 | 模块 |
|----------|------|
| 实现 O(1) 的 LRU 缓存 | [D1](../modules/D1_lru_cache) |
| 写一个线程安全的单例。局部静态变量线程安全吗？ | [D3](../modules/D3_singleton) |
| 双检锁（DCLP）为什么曾经是错的？ | [D3](../modules/D3_singleton) |
| 虚函数怎么实现的？vptr/vtable 在哪？ | [D4](../modules/D4_object_model) |
| 为什么基类析构函数要是 virtual？ | [D4](../modules/D4_object_model) |
| 什么是对象切片 (object slicing)？ | [D4](../modules/D4_object_model) |
| 含虚函数的对象 `sizeof` 是多少？ | [D4](../modules/D4_object_model) |

---

## 复习节奏建议

- **第一遍**：按 A→B→C→D 顺序实现，重在「让测试变绿」，理解原理。
- **第二遍**：合上代码，对着每个模块 README 的「面试追问」自述答案。
- **冲刺期**：只看本索引，逐题口述；卡壳的回到对应模块重读 TODO 注释。
