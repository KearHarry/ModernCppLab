# A2 · 智能指针 (Smart Pointers)

难度 ⭐⭐⭐ · 预计 3h（本课程最硬核的模块之一，建议分两次做完）

## 为什么大厂爱考
智能指针是 C++ 内存管理的「标准答案」，也是面试区分度最高的题之一。手写 `shared_ptr` 能一次性考查：RAII、引用计数、原子操作、模板、移动语义、循环引用——几乎覆盖现代 C++ 的半壁江山。能把控制块和 strong/weak 计数讲清楚，面试官基本就认可你的功底了。

## 背景知识

### 1) RAII：资源获取即初始化
对象**构造时**获取资源（内存/锁/句柄），**析构时**释放。把资源的生命周期绑定到栈对象上，离开作用域自动清理，再也不用记着手动 `delete`。智能指针就是 RAII 管理「堆内存」的典范。

### 2) `unique_ptr`：独占所有权
一块内存同一时刻只有一个 `unique_ptr` 拥有它。所以它 **不能拷贝**（拷贝会造成两个对象都认为自己拥有 → double free），**只能移动**（所有权转移）。零开销——和裸指针一样大、一样快。

### 3) `shared_ptr`：共享所有权 + 引用计数
多个 `shared_ptr` 可以共同拥有同一个对象。背后有一个**控制块 (control block)** 记录「现在有几个人在用」：
- **强引用计数 (strong/use count)**：有几个 `shared_ptr` 在持有对象。归 0 → 销毁对象。
- **弱引用计数 (weak count)**：有几个 `weak_ptr` 在观察。约定上「所有 shared_ptr 合起来再贡献 1」。

两阶段销毁是关键：**strong 归 0 → 销毁被管理对象**；**weak 也归 0 → 回收控制块本身**。本模块用「对象与控制块分开 new」的方式，正好能清楚看到这两个阶段。

### 4) 为什么引用计数要用原子 (`std::atomic`)
多个线程可能同时拷贝/析构指向同一对象的 `shared_ptr`，对计数的 `++/--` 必须是原子的，否则会丢更新 → 提前释放或泄漏。注意：**计数是线程安全的，但被管理的对象本身不是**。

### 5) `weak_ptr`：打破循环引用
两个对象用 `shared_ptr` 互相指向 → 计数永远不归 0 → **内存泄漏**（经典面试坑）。把其中一条边换成 `weak_ptr`（不增加 strong 计数，只观察），环就断了。用的时候 `lock()` 提升成 `shared_ptr`：对象还活着就拿到、已销毁就拿到空。

### 6) `make_shared` 的优势（了解）
`std::make_shared<T>(...)` 把「对象」和「控制块」一次性分配在一块内存里：少一次 `new`、cache 更友好、异常安全。代价：只要还有 `weak_ptr` 存活，那块大内存（含对象空间）就不能释放。本模块为教学清晰用的是分开分配，理解概念即可。

## 你要实现什么
打开 [include/smart_ptr.hpp](include/smart_ptr.hpp)，共 15 个 `TODO`：

| 区块 | 编号 | 任务 |
|------|------|------|
| UniquePtr | A2-1~5 | 移动构造、移动赋值、析构、`release`、`reset` |
| SharedPtr | A2-6~12 | 拷贝/移动 构造与赋值、析构、`reset`、`release_`（两阶段销毁核心） |
| WeakPtr | A2-13~15 | 从 `SharedPtr` 构造、`release_weak_`、`lock()`（CAS 提升） |

> 建议顺序：先把 `UniquePtr` 全部做完跑绿，再做 `SharedPtr`（先写 `release_` 这个核心），最后 `WeakPtr`。

## 关键坑
- **`unique_ptr` 不能拷贝**：拷贝构造/赋值要 `= delete`（已给出），只实现移动。
- **`shared_ptr` 的计数必须原子**：用 `fetch_add` / `fetch_sub`。注意 `fetch_sub` 返回的是「减之前」的旧值，判断是否归 0 要用 `== 1`。
- **两阶段销毁别合并**：strong 归 0 只销毁对象，不能立刻删控制块（可能还有 weak_ptr 在看）。
- **赋值要自赋值安全**：`p = p` 不能把自己提前释放。
- **`lock()` 要用 CAS 循环**：不能简单地「先判 strong>0 再 ++」——判断和自增之间对象可能就被别的线程销毁了（竞态）。要用 `compare_exchange` 原子地「仅当 strong>0 时 +1」。

## 如何验证
```powershell
cmake --build build -j
ctest --test-dir build -R A2 --output-on-failure
```
测试覆盖：`unique_ptr` 不可拷贝（`static_assert`）、移动转移所有权、`release/reset` 行为；`shared_ptr` 计数随拷贝/析构增减、归 0 销毁对象；`weak_ptr` 的 `lock/expired`、不延长对象寿命；以及 `weak_ptr` 打破循环引用（用存活对象计数验证无泄漏）。

## 面试追问

1. `shared_ptr` 的引用计数存在哪？为什么不存在对象里、也不存在指针里？

   **答**：存在堆上一块独立的**控制块**（control block）里，与被管理对象分开。不能存在对象里——对象可能在计数归零前还不该被销毁、且不是所有被指类型都能改造加计数字段；不能存在指针里——多个 `shared_ptr` 副本必须看到**同一个**计数，存指针里就各存各的、无法共享。控制块通常含 strong 计数、weak 计数、删除器和分配器。

2. `shared_ptr` 是线程安全的吗？（计数 vs 对象，分开答）

   **答**：**控制块的引用计数**是线程安全的——多线程同时拷贝/析构指向同一对象的**不同副本**，计数走原子操作不会出错。但**被管理的对象本身**毫无保护，多线程读写它要自己加锁。此外，多线程同时读写**同一个 `shared_ptr` 实例**（而非各自副本）也不安全，需要 `atomic<shared_ptr>` 或加锁。

3. `make_shared` 和 `shared_ptr<T>(new T)` 有什么区别？各自优劣？

   **答**：`make_shared` 把**控制块和对象一次性分配在同一块内存**，少一次堆分配、局部性更好，且无"new 成功但 shared_ptr 构造失败"的泄漏窗口。缺点：对象与控制块同生共死，只要还有 `weak_ptr` 存活那整块内存（含对象空间）就不能释放，也无法自定义删除器。`shared_ptr(new T)` 两次分配、可指定删除器、strong 归零即可释放对象内存。

4. 循环引用为什么泄漏？`weak_ptr` 怎么解决？`lock()` 为什么必要？

   **答**：两对象用 `shared_ptr` 互指，彼此 strong 计数都因对方 ≥1，谁都等不到归零，析构永不触发→泄漏。把一个方向改成 `weak_ptr`（它**不增加 strong 计数**）即可打破环。用时必须 `lock()` 把 weak 提升为 shared：对象还活着就拿到有效 shared、已销毁就拿到空——不能直接解引用 weak，因为它随时可能失效。

5. `enable_shared_from_this` 是干嘛的？为什么不能直接 `shared_ptr<T>(this)`？

   **答**：让被 `shared_ptr` 管理的对象在成员函数里安全拿到指向自己的 `shared_ptr`。直接 `shared_ptr<T>(this)` 会创建**第二个独立控制块**，两套计数各自归零→对象被 double free。`enable_shared_from_this` 在对象内藏了个 `weak_ptr`，对象首次被 shared 接管时绑定，`shared_from_this()` 由它 `lock()` 出来、与原控制块共享计数。

6. `weak_ptr::expired()` 之后再 `lock()`，中间对象被销毁了会怎样？

   **答**：这正是"先查 `expired()` 再解引用"不可取的原因——查和用之间对象可能被别的线程销毁（TOCTOU 竞争）。`lock()` 把"检查存活 + strong 计数 +1"做成一个**原子**操作（内部 CAS：仅当 strong 当前 >0 才成功 +1）：要么原子地拿到保证有效的 shared、要么拿到空。所以正确写法永远是 `if (auto sp = w.lock()) { 用 sp }`，而不是 `if (!w.expired())`。

7. `unique_ptr` 能放进 `vector` 吗？`vector<unique_ptr<T>>` 扩容时发生什么？

   **答**：能，且很常用。`unique_ptr` 不可拷贝但可移动，`vector` 扩容时**移动**而非拷贝元素（前提是其移动为 `noexcept`，它确实是）：每个元素被移动到新缓冲区、旧的置空，所有权平滑转移，被指对象本身不动。正是 C++11 的移动语义才让"容器装 `unique_ptr`"成为可能。
