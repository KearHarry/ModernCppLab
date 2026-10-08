# C++ 面试考点速查索引

> 面试前快速复盘：从问题反查实验模块。不要只背结论——至少能说出定义、适用条件、复杂度/代价、常见错误，并能在对应模块里写出关键代码。

## 现代 C++：值语义、类型系统与抽象

| 高频问题 | 对应模块 |
|---|---|
| `std::move` 真的移动了吗？移动后对象还能做什么？ | [A1](../modules/A1_move_semantics) |
| 转发引用、引用折叠与完美转发是什么关系？ | [A1](../modules/A1_move_semantics) / [A7](../modules/A7_type_deduction) |
| Rule of Three / Five / Zero；移动构造为何常标 `noexcept`？ | [A1](../modules/A1_move_semantics) / [A9](../modules/A9_exception_safety) |
| `unique_ptr`、`shared_ptr`、`weak_ptr` 的所有权语义分别是什么？ | [A2](../modules/A2_smart_pointers) |
| `shared_ptr` 控制块有什么？`make_shared` 有什么取舍？ | [A2](../modules/A2_smart_pointers) |
| 循环引用、`enable_shared_from_this`、别名构造分别解决什么？ | [A2](../modules/A2_smart_pointers) |
| SFINAE、`void_t`、concepts 如何做约束？为什么 concepts 诊断更好？ | [A3](../modules/A3_template_metaprogramming) |
| 模板偏特化、可变参模板、折叠表达式怎么工作？ | [A3](../modules/A3_template_metaprogramming) |
| 类型擦除和继承多态、模板静态多态有什么区别？ | [A4](../modules/A4_type_erasure) / [D2](../modules/D2_crtp) |
| `std::function` 如何存任意 callable？什么是 SBO？ | [A4](../modules/A4_type_erasure) |
| `optional` 怎样在没有堆分配时表达“可能无值”？ | [A5](../modules/A5_optional) |
| union 活跃成员、placement new、显式析构分别意味着什么？ | [A5](../modules/A5_optional) / [A8](../modules/A8_variant_visit) |
| 协程帧、`promise_type`、挂起/恢复、惰性生成如何配合？ | [A6](../modules/A6_coroutine_generator) |
| `auto`、`decltype`、`decltype(auto)` 推导有什么差别？ | [A7](../modules/A7_type_deduction) |
| 传值/引用模板如何处理 cv、数组和函数退化？ | [A7](../modules/A7_type_deduction) |
| 重载决议中精确匹配、提升、转换、模板哪个优先？ | [A7](../modules/A7_type_deduction) |
| `variant` 如何记录活跃类型？`visit` 如何分派？ | [A8](../modules/A8_variant_visit) |
| 什么是 `valueless_by_exception`？怎样写异常安全的换型赋值？ | [A8](../modules/A8_variant_visit) / [A9](../modules/A9_exception_safety) |
| 基本、强、不抛异常保证分别承诺什么？ | [A9](../modules/A9_exception_safety) |
| 栈展开时析构做什么？为什么析构函数不能逃出异常？ | [A9](../modules/A9_exception_safety) |
| Scope Guard、copy-and-swap、先准备后提交如何实现回滚？ | [A9](../modules/A9_exception_safety) |
| lambda 捕获的对象是什么？`[&]` 为什么容易悬垂？ | [A10](../modules/A10_lambda_invoke) |
| `std::invoke` 如何统一函数、函数对象与成员指针？ | [A10](../modules/A10_lambda_invoke) |

## 并发：同步、生命周期与内存模型

| 高频问题 | 对应模块 |
|---|---|
| 读者优先、写者优先、公平读写锁各有什么饥饿问题？ | [B1](../modules/B1_rwlock) |
| 条件变量为什么必须配谓词循环？什么是虚假唤醒？ | [B2](../modules/B2_blocking_queue) |
| 阻塞队列怎样处理背压、关闭以及被唤醒的生产者/消费者？ | [B2](../modules/B2_blocking_queue) |
| 原子与 mutex 的区别？CAS 为何常写成循环？ | [B3](../modules/B3_atomics_memory_order) |
| 六种 `memory_order` 的保证与典型使用场景是什么？ | [B3](../modules/B3_atomics_memory_order) / [B7](../modules/B7_memory_model) |
| 自旋锁何时优于 mutex？false sharing 如何产生？ | [B3](../modules/B3_atomics_memory_order) |
| Treiber stack 的 ABA 与内存回收难题是什么？ | [B4](../modules/B4_lockfree_stack) |
| hazard pointer、epoch reclamation、tagged pointer 各解决哪一层问题？ | [B4](../modules/B4_lockfree_stack) |
| 线程池如何包装返回值和异常？`future/promise/packaged_task` 什么关系？ | [B5](../modules/B5_thread_pool) |
| 线程池析构时如何 drain/cancel？0 个 worker 会发生什么？ | [B5](../modules/B5_thread_pool) |
| `thread`、`jthread` 的析构语义；join 与 detach 如何选择？ | [B6](../modules/B6_thread_lifecycle) |
| `stop_token` 如何做协作式取消？阻塞等待怎样响应停止？ | [B6](../modules/B6_thread_lifecycle) |
| 死锁四个必要条件是什么？`std::lock/scoped_lock` 如何避免锁顺序死锁？ | [B6](../modules/B6_thread_lifecycle) |
| data race 为什么是 UB，而“结果不确定”远远不够？ | [B7](../modules/B7_memory_model) |
| sequenced-before、synchronizes-with、happens-before 如何串起来？ | [B7](../modules/B7_memory_model) |
| release/acquire 发布数据与 release/acquire fence 有何不同？ | [B7](../modules/B7_memory_model) |

## STL：容器、内存与泛型算法

| 高频问题 | 对应模块 |
|---|---|
| `vector` 的 size/capacity、几何扩容与均摊 O(1) 如何解释？ | [C1](../modules/C1_vector) |
| `reserve`/`resize` 区别；扩容怎样保证异常安全？ | [C1](../modules/C1_vector) / [A9](../modules/A9_exception_safety) |
| SSO 的布局、收益和移动/拷贝陷阱是什么？ | [C2](../modules/C2_string_sso) |
| free-list 内存池如何做到 O(1)？怎样处理对齐和碎片？ | [C3](../modules/C3_memory_pool) |
| 哈希冲突、负载因子、rehash、最坏 O(n) 如何解释？ | [C4](../modules/C4_hash_table) |
| `map` 与 `unordered_map` 如何选择？引用和迭代器何时失效？ | [C4](../modules/C4_hash_table) / [C7](../modules/C7_red_black_tree) |
| 侵入式链表为何零额外分配？hook 生命周期由谁负责？ | [C5](../modules/C5_intrusive_list) |
| 跳表为什么期望 O(log n)？与平衡树相比有何取舍？ | [C6](../modules/C6_skip_list) |
| 红黑树五条不变量是什么？旋转为什么不破坏有序性？ | [C7](../modules/C7_red_black_tree) |
| 红黑树、AVL 的查询/更新和工程取舍是什么？ | [C7](../modules/C7_red_black_tree) |
| `deque` 为什么能双端 O(1) 且支持随机访问？ | [C8](../modules/C8_deque) |
| 分段存储与 vector 连续存储对缓存、扩容和失效规则有何影响？ | [C8](../modules/C8_deque) |
| Allocator 的 allocate 与对象构造为何分离？ | [C9](../modules/C9_allocator_pmr) |
| `allocator_traits`、rebind、传播 traits、allocator 相等性是什么？ | [C9](../modules/C9_allocator_pmr) |
| PMR 如何运行时切换资源？`monotonic_buffer_resource` 适合什么？ | [C9](../modules/C9_allocator_pmr) |
| input/forward/bidirectional/random-access/contiguous 迭代器如何递进？ | [C10](../modules/C10_iterators_ranges) |
| `[first,last)` 有什么好处？sentinel 为什么可与 iterator 异型？ | [C10](../modules/C10_iterators_ranges) |
| `lower_bound` 返回什么？projection 与 comparator 有何区别？ | [C10](../modules/C10_iterators_ranges) |

## 对象模型与工程机制

| 高频问题 | 对应模块 |
|---|---|
| 如何用哈希表 + 双向链表实现 O(1) LRU？ | [D1](../modules/D1_lru_cache) |
| CRTP 静态多态与虚函数动态多态如何选择？ | [D2](../modules/D2_crtp) / [D4](../modules/D4_object_model) |
| Meyers singleton 为什么线程安全？静态初始化顺序灾难是什么？ | [D3](../modules/D3_singleton) / [D8](../modules/D8_compilation_linking) |
| vptr/vtable 是什么？哪些是常见 ABI 实现而非标准保证？ | [D4](../modules/D4_object_model) |
| 为什么多态基类析构通常必须 virtual？什么是对象切片？ | [D4](../modules/D4_object_model) |
| Pimpl 如何降低编译耦合并改善 ABI 稳定性？ | [D5](../modules/D5_pimpl) |
| `unique_ptr<Incomplete>` 的析构为何常要放进 `.cpp`？ | [D5](../modules/D5_pimpl) |
| 基类、成员、派生类的构造/析构顺序由什么决定？ | [D6](../modules/D6_lifetime_layout) |
| padding、`alignof`、EBO、`[[no_unique_address]]` 怎样影响布局？ | [D6](../modules/D6_lifetime_layout) |
| 临时对象的生命周期何时延长？返回值优化何时发生？ | [D6](../modules/D6_lifetime_layout) |
| 四种 named cast 各表达什么意图？为什么少用 C 风格转换？ | [D7](../modules/D7_casts_type_safety) |
| `dynamic_cast` 指针/引用失败分别怎样表现？ | [D7](../modules/D7_casts_type_safety) |
| `reinterpret_cast` 能否绕过严格别名？什么时候用 `bit_cast`/`memcpy`？ | [D7](../modules/D7_casts_type_safety) |
| 声明、定义、翻译单元、内部/外部链接分别是什么？ | [D8](../modules/D8_compilation_linking) |
| ODR 为什么存在？头文件函数/变量何时需要 `inline`？ | [D8](../modules/D8_compilation_linking) |
| 模板为什么通常定义在头文件？`extern "C"` 做了什么？ | [D8](../modules/D8_compilation_linking) |
| API 与 ABI 有何区别？改私有成员为什么可能破坏 ABI？ | [D5](../modules/D5_pimpl) / [D8](../modules/D8_compilation_linking) |

## 引擎 / 游戏与性能设施

| 高频问题 | 对应模块 |
|---|---|
| index+generation 句柄如何防止 use-after-free 式旧句柄？ | [E1](../modules/E1_slot_map) |
| arena/bump allocator 为什么快？marker/rewind 如何工作？ | [E2](../modules/E2_arena_allocator) |
| 信号-槽怎样处理连接 ID、断连与回调中的重入修改？ | [E3](../modules/E3_delegate) |
| 编译期 FNV-1a、UDL 与字符串 ID 有什么收益和碰撞风险？ | [E4](../modules/E4_string_id) |
| AoS 与 SoA 怎样按访问模式影响 cache line 利用率？ | [E5](../modules/E5_data_oriented_design) |
| 为什么性能单测不应断言固定耗时？可用哪些确定性指标？ | [E5](../modules/E5_data_oriented_design) |
| Sparse set 如何同时做到 O(1) 查询和连续遍历？ | [E6](../modules/E6_sparse_set_ecs) |
| swap-and-pop 有什么失效代价？怎样求两个组件池交集？ | [E6](../modules/E6_sparse_set_ecs) |

## 冲刺时的统一答题模板

遇到任何 C++ 机制题，可按下面顺序组织答案：

1. **语义**：标准承诺了什么，哪些只是某个 ABI/标准库的常见实现。
2. **机制**：对象、内存、线程或符号在底层怎样协作。
3. **复杂度与代价**：时间、空间、分配、间接跳转、缓存、同步成本。
4. **失效与 UB**：生命周期、迭代器、数据竞争、异常路径有哪些前置条件。
5. **取舍**：什么场景应该用，什么场景有更简单或更安全的方案。

如果一道题只能说出“它更快”或“底层是某结构”，回到对应模块做一遍 TODO，并让自己能解释每个测试为什么存在。
