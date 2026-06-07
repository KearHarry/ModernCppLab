# B3 · 原子操作与内存序 (Atomics & Memory Order)

难度 ⭐⭐⭐ · 预计 2h

## 为什么大厂爱考
`std::atomic` 和内存序是并发的"硬核"部分，也是无锁编程的地基。面试官借它考查：什么是 CAS、为什么无锁更新要写成"读—算—CAS 重试"的循环、`memory_order` 六种语义分别是什么、acquire/release 怎么配对、自旋锁和互斥锁怎么选。亲手写一个自旋锁和一个 CAS 循环，这些点就从"背概念"变成"会推理"。

## 背景知识

### 1) `std::atomic<T>`：不可分割的读写
普通变量在多线程下 `++x` 会丢更新（读、加、写三步可能交错）。`std::atomic<T>` 把这些操作做成**原子**的：`load() / store() / fetch_add() / exchange() / compare_exchange_*()` 不会被其它线程打断，也不会"读到一半"。

### 2) `std::atomic_flag`：最简单的原子布尔
只有两个操作，是自旋锁的天然材料：
- `test_and_set(order)`：把 flag 置 `true`，**返回它之前的值**。返回 `false` 表示"之前没人占，我抢到了"；返回 `true` 表示"已被占用"。
- `clear(order)`：把 flag 置回 `false`（释放）。

> C++20 起 `atomic_flag` 默认就是清零状态，**不再需要** `ATOMIC_FLAG_INIT`。

### 3) CAS：`compare_exchange_weak(expected, desired)`
"比较并交换"，无锁算法的核心。语义：**当且仅当**原子变量当前值等于 `expected` 时，把它改成 `desired` 并返回 `true`；否则把 `expected` 更新为变量的最新值并返回 `false`。

无锁更新的通用范式：
```cpp
T cur = a.load();              // 1) 读当前值
while (!a.compare_exchange_weak(cur, f(cur))) {
    // 2) CAS 失败说明被别人改了，cur 已是最新值，循环重试
}
```
- `compare_exchange_weak` 在某些平台可能**伪失败**（值没变也返回 false），所以必须放在循环里；换来更高的效率。
- `compare_exchange_strong` 不伪失败，但单次开销略大；用于不方便循环的场景。

### 4) `memory_order`：编译器/CPU 的重排约束
为了性能，编译器和 CPU 会重排指令。内存序就是你对"重排到什么程度可接受"的声明：

| 内存序 | 语义 | 典型用途 |
|--------|------|----------|
| `relaxed` | 只保证本变量原子性，不约束与其它读写的顺序 | 纯计数器（如引用计数 +1） |
| `acquire` | 读屏障：本次读**之后**的操作不会被重排到它之前 | 加锁、读"就绪标志" |
| `release` | 写屏障：本次写**之前**的操作不会被重排到它之后 | 解锁、置"就绪标志" |
| `acq_rel` | 读改写操作同时具备 acquire + release | `fetch_sub` 减引用计数、CAS |
| `seq_cst` | 最强：所有线程看到同一个全局顺序 | 默认值，拿不准就用它 |

### 5) acquire / release 配对（最重要的直觉）
```
线程 A:  data = 42;                 // (1) 普通写
         ready.store(true, release); // (2) release 写：保证 (1) 排在 (2) 前

线程 B:  while(!ready.load(acquire)); // (3) acquire 读
         assert(data == 42);          // (4) 读到 ready=true 就一定看得到 data=42
```
**release 写**之前的所有写，对**读到该值的 acquire 读**之后都可见。这正是"解锁→加锁"在内存层面的含义。

## 你要实现什么
打开 [include/atomics.hpp](include/atomics.hpp)，共 4 个 `TODO`：

| 编号 | 函数 | 任务 |
|------|------|------|
| B3-1 | `SpinLock::lock()` | `while (test_and_set(acquire)) {}` 自旋抢锁 |
| B3-2 | `SpinLock::unlock()` | `clear(release)` 释放锁 |
| B3-3 | `SpinLock::try_lock()` | `return !test_and_set(acquire);` 尝试一次 |
| B3-4 | `atomic_fetch_max()` | CAS 循环把 target 更新为 max(target, value)，返回旧值 |

## 关键坑
- **抢锁用 `acquire`、放锁用 `release`**：这样临界区的读写不会"漏"到锁外面去。
- **CAS 必须写成循环**：`compare_exchange_weak` 会伪失败；失败时 `expected` 已被刷新为最新值，直接重试即可，不要重新 `load`。
- **`fetch_max` 返回的是"更新前"的旧值**（和 `fetch_add` 一致的约定），不是新值。
- **自旋锁只适合临界区极短的场景**：自旋是"忙等"烧 CPU；临界区长或可能阻塞时该用 `std::mutex`（会让出 CPU 睡眠）。
- **别忘了 `unlock()`**：自旋锁没有 RAII 包装时，异常路径上忘了解锁会让其它线程**永久自旋**（死锁）。

## 如何验证
```powershell
ctest --test-dir build -R B3 --output-on-failure
```
测试覆盖：`try_lock` 的抢占/释放语义；8 线程 × 20000 次自旋锁保护累加，结果精确等于 160000（证明真正互斥）；`fetch_max` 单线程返回值与更新效果；16 线程并发抬高最大值，最终等于已知最大值（证明 CAS 不丢更新）。

> 提示：骨架里 `try_lock` 桩"假装总抢到锁"、`fetch_max` 桩"只读不更新"，所以一开始测试只失败、不卡死。实现后才会出现真正的互斥/原子行为。

## 面试追问

1. CAS 是什么？为什么无锁更新要写成循环？什么是 ABA 问题？

   **答**：CAS（compare-and-swap）是一条原子指令：`若内存当前值==期望值，就写入新值并返回成功，否则把现值读回期望值并返回失败`。无锁更新要写成循环，是因为"读旧值→算新值→CAS 写回"之间别的线程可能已改动，CAS 会失败；失败时拿最新值重算、重试，直到成功。**ABA 问题**：值从 A 改成 B 又改回 A，CAS 只比较值、看到还是 A 就误以为"没人动过"，但中间状态已变（如被 pop 又 push 回的节点已被释放重用），导致逻辑出错。解法是给值附带版本号/标签（tagged pointer）或用 hazard pointer。

2. `compare_exchange_weak` 和 `strong` 有何区别？什么时候用哪个？

   **答**：`weak` 允许**伪失败**（spurious failure）——即使值匹配也可能返回 false，但在 LL/SC 架构（ARM、PowerPC）上能映射成更高效的单条指令；`strong` 保证只在值真不匹配时才失败。本来就有重试循环时用 `weak`（伪失败下次循环自然重试，省一层内部循环、更快）；没有循环、CAS 只调用一次、伪失败会导致逻辑错误的地方用 `strong`。x86 上两者代价基本相同，差别主要体现在 LL/SC 平台。

3. `memory_order` 六种分别是什么语义？acquire/release 怎么配对？

   **答**：`relaxed`（只保证本操作原子，不约束周围读写的可见性/顺序）；`acquire`（读屏障：其后访问不能重排到它前，且能看到对应 release 之前的所有写）；`release`（写屏障：其前访问不能重排到它后，把之前的写"发布"出去）；`acq_rel`（读改写操作同时具备 acquire+release）；`consume`（弱化的 acquire，只约束数据依赖链，实践少用、常被提升为 acquire）；`seq_cst`（最强，在原子序之上再加全局单一全序，最易推理但最贵）。**配对**：A 对某变量 `store(release)`、B 对同一变量 `load(acquire)` 且读到了 A 写的值，则 A 在 release 之前的所有写对 B 在 acquire 之后都可见——这就是"同步于"(synchronizes-with) 关系，是无锁传递数据的基石。

4. 自旋锁和互斥锁怎么选？自旋锁的优缺点？什么是"自适应自旋"？

   **答**：临界区**极短**、竞争**不激烈**、不能睡眠（如中断上下文）时用自旋锁——抢不到就忙等几纳秒，省掉陷入内核/上下文切换；临界区**长**或可能阻塞时用互斥锁——抢不到就让出 CPU 睡眠，避免空转烧 CPU。自旋锁优点是无系统调用、延迟低；缺点是忙等纯烧 CPU，持锁线程被抢占会让等待者空转很久（单核/超线程上尤其糟）。**自适应自旋**是折中：先自旋一小段试图抢到（赌锁很快释放），超阈值还没拿到就转睡眠，兼顾低延迟与不浪费 CPU——`std::mutex` 多数实现就这么做。

5. `relaxed` 用在引用计数 `+1` 安全，为什么 `-1`（释放）却要用 `acq_rel`？

   **答**：`+1`（如 shared_ptr 拷贝）安全用 `relaxed`，因为新增引用者本就已持有该对象的有效引用，增计数无需与其他操作建立顺序，只要原子不丢更新即可。`-1`（释放）不行：减到 0 的线程要去析构并释放对象，它必须**看到其他线程在各自释放前对对象的所有写**（否则析构时读到脏数据），并保证自己的析构不被重排到减计数之前。所以减计数用 `acq_rel`（或减用 `release`、判零后补一个 `acquire` 栅栏）：release 把本线程之前的写发布出去，acquire 让归零者看到所有人的写，二者合起来才能安全销毁。

6. 什么是 false sharing（伪共享）？如何用 cache line 对齐缓解？

   **答**：**伪共享**指两个本无关的变量恰好落在同一条 cache line（通常 64 字节）上，被不同核分别频繁写——硬件按整条 line 维护一致性，一个核写就使另一核缓存的整条 line 失效，导致 line 在核间反复弹跳（cache ping-pong），性能暴跌，尽管逻辑上互不相干。缓解：用 `alignas(64)`（或 `std::hardware_destructive_interference_size`）把各自独立的热点变量对齐/填充到独立 cache line，让它们不再共享同一行；如每线程计数器各占一行、或给并发写的字段加 padding。
