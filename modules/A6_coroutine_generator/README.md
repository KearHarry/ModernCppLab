# A6 · C++20 协程 Generator&lt;T&gt;（惰性序列）

难度 ⭐⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考
C++20 协程是近年最大的语言特性，也是面试新宠。但 `co_await` 的异步全貌太大，**`Generator`（惰性序列）是协程最小、最好懂的入口**：函数里 `co_yield` 一个一个吐值，调用方取一个才算一个——能表达**无限序列**而不爆内存。答得清 `promise_type`、`coroutine_handle`、`initial/final_suspend` 怎么协作，就证明你真懂协程的"状态机"本质，而不只是会用库。这也是 Python `yield`、C# `IEnumerator`、JS `function*` 的 C++ 对应物。

## 背景知识

### 1) 协程 = 能"暂停/恢复"的函数
普通函数一次跑到底。**协程**可以跑到一半 `co_yield` 出一个值就**挂起**，把控制权还给调用方；下次 `resume()` 再从挂起点**继续**。一个函数体里只要出现 `co_yield` / `co_await` / `co_return`，编译器就把它整体改写成一个**状态机**，并在堆上分配一个**协程帧**保存局部变量与"当前停在哪"。

### 2) promise_type：协程与外界的"接线盒"
编译器在协程帧里放一个你定义的 `promise_type` 对象，并在固定时机回调它的固定成员：
```
get_return_object()   协程刚启动 → 造出返回给调用方的对象（这里是 Generator）
initial_suspend()     启动后是否立刻挂起（suspend_always = 先停在入口，惰性）
yield_value(v)        每次 co_yield v → 存下 v，然后挂起
final_suspend()       协程结束后是否挂起（suspend_always = 让外部还能读状态）
return_void()         co_return; 或落到末尾时调用
unhandled_exception() 体内抛了没接住的异常时调用
```
返回类型 `Generator<T>` 必须有一个名为 `promise_type` 的内嵌类型——这是编译器约定的"暗号"。

### 3) coroutine_handle：协程帧的"遥控器"
```cpp
std::coroutine_handle<promise_type> h;
h.resume();              // 续跑到下一个挂起点
h.done();                // 是否已结束
h.destroy();             // 销毁协程帧（释放内存）
h.promise();             // 拿到帧里的 promise 对象（读 current_/exc_）
handle::from_promise(p); // 反向：由 promise 拿到 handle
```

### 4) 惰性怎么来的？
`initial_suspend` 返回 `suspend_always` → 协程一启动就停在入口，**一个值都没算**。之后每 `resume()` 一次，跑到下一个 `co_yield` 又停下交出值。于是"取一个、算一个"，`naturals()`、`fib()` 这种无限序列也能安全地只取前 N 个。

### 5) 套上迭代器，让 range-for 能用
```cpp
begin()      // resume 一次推进到第一个值，返回包着 handle 的 iterator
operator++   // 再 resume 推进到下一个；协程 done() 了就等于末尾
operator!=   // 用 handle.done() 判断到没到末尾；end() 用 std::default_sentinel
```
迭代器、`begin/end`、移动/析构本模块**已给好**，且对"空 handle"做了保护——所以骨架阶段遍历恒为空，既不崩也不死循环。

## 你要实现什么
打开 [include/generator.hpp](include/generator.hpp)。迭代器、`begin/end`、`initial/final_suspend`、移动构造/赋值、析构都已给好。你实现 `promise_type` 里的 3 个 `TODO`（协程的"接线盒"）：

| 编号 | 成员 | 任务 |
|------|------|------|
| A6-1 | `get_return_object` | `from_promise(*this)` 拿到 handle，包进 `Generator` 返回 |
| A6-2 | `yield_value(v)` | `current_ = std::move(v); return {};`（存值并挂起）|
| A6-3 | `unhandled_exception` | `exc_ = std::current_exception();`（抓住异常留待 rethrow）|

## 关键坑
- **`Generator` 必须叫 `promise_type`**：这是编译器约定的名字，拼错就不被识别为协程返回类型。
- **`final_suspend` 必须 `noexcept`**：标准强制要求，否则编译报错。
- **`final_suspend` 返回 `suspend_always` 才能"事后读状态"**：若返回 `suspend_never`，协程结束即自毁，外部再 `h.promise()`/`h.done()` 就是访问已销毁的帧。本模块用 `suspend_always`，由 `Generator` 析构统一 `destroy()`。
- **生命周期归 Generator 管**：协程帧不会自动释放，必须在析构里 `h.destroy()`；只可移动，移动后把源 handle 置空，避免**双重 destroy**。
- **异常要"接力"**：体内异常先被 `unhandled_exception` 抓进 `exc_`，等迭代到 `done()` 时由 `begin/operator++` `rethrow_exception` 抛给调用方——别在协程里直接让它逃逸。
- **MinGW/GCC 需 `-std=c++20`**：协程是 C++20 特性；本仓库已统一开启。

## 如何验证
```powershell
ctest --test-dir build -R A6 --output-on-failure
```
测试覆盖：按序 `co_yield`（1,2,3）；半开区间 `[lo,hi)` 与空区间；**惰性**——无限自然数/斐波那契只取前 N 个不卡死；协程体内**异常传播**到调用方；**只可移动**——移动后源被掏空、目标仍可遍历。

> 提示：骨架里 `get_return_object` 返回空盒子（无 handle）、`yield_value` 不存值、`unhandled_exception` 不抓异常，于是遍历恒为空。空 handle 的迭代器**立刻等于末尾**，所以无限序列的用例也不会死循环，全程只变红、不崩溃。

## 面试追问

1. 一个函数怎么就成了协程？编译器对它做了什么改写？

   **答**：函数体里只要出现 `co_await`/`co_yield`/`co_return` 之一，它就是协程。编译器把它改写成**状态机**：在堆上分配一个**协程帧**保存局部变量、形参、当前挂起点；把函数按恢复点切成片段；按 `promise_type` 协议插入对 `get_return_object`/`initial_suspend`/`yield_value`/`final_suspend` 的调用。调用协程返回的是 promise 造出的"返回对象"（这里是 `Generator`），函数体不立即跑完。

2. `promise_type` 各成员分别在什么时机被调用？画一遍 `co_yield` 的完整时序。

   **答**：调用协程→分配帧、构造 promise→`get_return_object()` 造出 Generator 交给调用方→`initial_suspend()`（这里 `suspend_always`，立刻挂起，惰性）。之后每次 `resume()`：体跑到 `co_yield v`→调 `yield_value(v)` 把 v 存进 promise、返回 `suspend_always` 再次挂起，控制权回调用方读值。如此往复；体结束→`final_suspend()`（`suspend_always`，保帧存活让外部查 done）→帧由 `Generator` 析构时 `destroy()` 释放。中途抛异常走 `unhandled_exception()`。

3. `initial_suspend`/`final_suspend` 返回 `suspend_always` vs `suspend_never` 各意味着什么？惰性来自哪一个？

   **答**：返回的是 awaiter：`suspend_always` 让协程在该点**挂起**（暂停、交还控制权），`suspend_never` **不挂起**（继续往下跑）。**惰性来自 `initial_suspend` 返回 `suspend_always`**——协程创建后立刻挂起、一行体代码都不执行，直到第一次 `resume()`（即 `begin()`）才产出第一个值。`final_suspend` 用 `suspend_always` 是为让帧结束后不自毁、外部还能查 `done()` 并安全析构。

4. 协程帧在哪分配、何时释放？为什么 `Generator` 要只可移动、析构里 `destroy()`？

   **答**：协程帧默认在**堆**上分配（编译器在能证明不逃逸时可优化掉，但一般保留）。它由 `coroutine_handle` 间接拥有，但 handle 是个"裸句柄"不管生命周期，所以需要 RAII 拥有者——`Generator` 在析构里 `handle.destroy()` 释放帧。它**只可移动不可拷贝**，因为一个帧只能有一个所有者；拷贝会让两个 Generator 都去 destroy 同一帧（double free），移动则转移句柄、把源置空。

5. 无限序列为什么不会算爆内存？惰性求值的代价是什么？

   **答**：因为**值按需产生**——`co_yield` 一次产一个、随即挂起，任意时刻帧里只存"当前状态"（如斐波那契的前两个数），绝不会把整条无限序列实体化；调用方取多少就 resume 多少。代价：每次 resume/挂起有**状态机切换开销**（保存/恢复、间接跳转），协程体**无法跨挂起点内联**，比普通循环慢，且有一次协程帧堆分配。

6. 协程体内抛异常，最终如何到达调用方？`unhandled_exception` 的作用？

   **答**：协程体里逃逸的异常被编译器插入的 catch 捕获并调用 `promise.unhandled_exception()`。本实现里它用 `std::current_exception()` 把异常**存进 promise**；随后协程走向 final_suspend 挂起。调用方下次 `resume()`/取值时，由我们在恢复后检查并 `std::rethrow_exception` **重新抛出**到调用方。没有这个机制，协程里的异常无处可去会直接 `std::terminate`。

7. 这个手写 `Generator` 和 C++23 的 `std::generator` 差在哪？

   **答**：`std::generator` 是标准化、功能完备版：支持 `co_yield` **嵌套生成器**（递归 yield，子序列摊平且 O(1) 转发）、产出**引用**元素避免拷贝、可定制**分配器**、与 `ranges` 无缝适配（本身是个 view）、异常与值类别处理更完善。手写版只演示最小内核：单层 yield、值语义、固定分配、简陋迭代器。

8. 协程 vs 线程 vs 状态机手写：各自的内存/切换开销与适用场景？

   **答**：**线程**有独立栈、OS 抢占式调度，切换走内核、开销最大（KB 级栈 + 上下文切换），适合真正并行/阻塞调用。**协程**是用户态协作式挂起，帧小（只存活跃状态）、切换是函数级跳转，适合海量并发 IO/生成器，但不能单独利用多核（需配线程池）。**手写状态机**零额外抽象开销、最省，但代码晦涩难维护——协程本质就是"编译器替你生成状态机"，用一点点开销换回可读性。
