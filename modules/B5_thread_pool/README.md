# B5 · 线程池 (Thread Pool)

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考
线程池是服务端开发的"标配组件"——Web 服务器、RPC 框架、数据库连接池底层都有它。它把前面学的东西串起来：条件变量（B2）、任务队列、再加上 `std::future` / `std::packaged_task` 这套异步结果机制。面试官借它考查：为什么要复用线程、`submit` 怎么把任意签名的任务统一入队、`future` 怎么拿回返回值、怎么优雅关闭不丢任务不泄漏线程。写出一个能跑的线程池，并发工程能力就有了说服力。

## 背景知识

### 1) 为什么要线程池
每来一个任务就 `new std::thread` 再 `join`，开销大（内核态切换、栈分配）且并发度不可控。线程池预先开好固定数量的线程**复用**：任务进队列排队，空闲线程取出来执行。好处是省开销、限并发（天然背压）。

### 2) `std::function<void()>`：把任务"装箱"
队列里要存"待执行的任务"，但任务签名五花八门。`std::function<void()>` 用**类型擦除**把它们统一成"无参数、无返回"的可调用对象——返回值则通过 `future` 旁路取回（见下）。

### 3) `std::packaged_task<R()>` + `std::future<R>`：拿回返回值
- `std::packaged_task<R()>` 包住"一个返回 R 的调用"，并内置一个 `std::future<R>`。**调用它**时，返回值会自动写进那个 future。
- `std::future<R>` 是"取货凭证"：`fut.get()` 会**阻塞等待**任务执行完，然后返回结果（若任务抛异常，`get()` 会重新抛出——异常也能跨线程传递）。

```cpp
std::packaged_task<int()> task([]{ return 42; });
std::future<int> fut = task.get_future();
task();                  // 在某个线程执行
int r = fut.get();       // 在另一个线程取回 42
```

### 4) 为什么用 `shared_ptr` 包 `packaged_task`
`packaged_task` **只能移动、不能拷贝**，而 `std::function` 要求其目标**可拷贝**。把 task 放进 `shared_ptr`，让队列里的 lambda **按值捕获这个 shared_ptr**（指针可拷贝），就绕过了这个限制：
```cpp
auto task = std::make_shared<std::packaged_task<R()>>(std::bind(f, args...));
tasks_.emplace([task]{ (*task)(); });   // lambda 可拷贝 → 能塞进 std::function
```

### 5) `std::invoke_result_t<F, Args...>`：推导返回类型
`submit` 是模板，得知道任务的返回类型 R 才能声明 `future<R>`。`std::invoke_result_t<F, Args...>`（C++17）就是"用 Args 调用 F 得到的返回类型"。

### 6) 优雅关闭（graceful shutdown）
析构时三步握手：**① 置 `stop_=true` → ② `notify_all` 唤醒所有等待线程 → ③ `join` 等它们干完退出**。关键在工作线程的**退出条件**：必须是"**已关闭且队列已空**"才退出，而不是"一看到 stop_ 就走"——否则关闭瞬间还在队列里的任务会被丢掉。

## 你要实现什么
打开 [include/thread_pool.hpp](include/thread_pool.hpp)。构造/析构（含关闭握手）已给出，你实现 2 个 `TODO`：

| 编号 | 函数 | 任务 |
|------|------|------|
| B5-1 | `submit(f, args...)` | 打包成 `packaged_task` → 取 `future` → 入队 → `notify_one` → 返回 `future` |
| B5-2 | `worker_loop()` | `wait` 取任务、**锁外执行**；"已关闭且队列空"才退出 |

## 关键坑
- **任务要在锁【外】执行**：`wait`/取任务时持锁，取到后**先解锁再 `task()``**。否则所有线程被一把锁串行化（失去并发意义），任务内若再 `submit` 还会**自死锁**。
- **退出条件是"`stop_` 且 队列空"**，不是只看 `stop_`——否则丢任务。
- **`packaged_task` 不可拷贝**：必须用 `shared_ptr` 包一层才能进 `std::function`。
- **`future.get()` 只能调用一次**：第二次会抛 `std::future_error`。需要多处取值就改用 `std::shared_future`。
- **构造时 `n>=1`**：否则没有线程执行任务，`get()` 永远阻塞。
- **任务抛出的异常**会被 `packaged_task` 捕获并存入 future，在 `get()` 时重新抛出——不会让工作线程崩溃。

## 如何验证
```powershell
ctest --test-dir build -R B5 --output-on-failure
```
测试覆盖：带返回值的任务能用 future 取回正确结果；200 个任务全部执行（计数精确）；100 个带返回值任务求和等于 5050（无丢失无重复）；优雅关闭——析构前入队的 50 个任务必须全部执行完。

> 提示：骨架里 `submit` 桩"返回默认值不真正执行"、`worker_loop` 桩为空，所以一开始测试只失败、不卡死。实现后才会出现真正的并发执行与关闭行为。

## 面试追问

1. 线程池的核心组成？为什么比"每任务一线程"好？

   **答**：核心是三件套：一组**长期存活的工作线程**、一个**线程安全的任务队列**、以及**同步原语**（mutex+cv 协调入队/取任务/关闭）。每个 worker 循环：等队列非空→取一个任务→锁外执行→回到等待。比"每任务一线程"好在：①省掉频繁创建/销毁线程的开销（线程创建涉及内核、栈分配，很贵）；②限制并发线程数，避免任务暴增时线程爆炸耗尽内存/疯狂上下文切换；③线程复用、缓存更热；④可统一做背压、关闭、监控。

2. `submit` 如何支持任意签名的任务并拿回返回值？`packaged_task` 和 `promise` 有何区别？

   **答**：`submit` 写成模板 `template<class F, class...Args>`，用 `std::packaged_task` 或 lambda 把调用打包成统一的 `std::function<void()>` 放进队列，同时返回它关联的 `std::future` 给调用方取结果。`packaged_task` 包住一个**可调用对象**，被调用时自动把返回值/异常塞进内部共享状态，适合"把一次函数调用变成 future"；`promise` 是更底层的**手动写入端**，由你显式 `set_value/set_exception`，调用方从 `promise.get_future()` 取——适合结果不是来自单次函数返回、而是异步事件/回调里手动设置的场景。线程池里 `packaged_task` 最顺手。

3. 为什么任务要在锁外执行？锁内执行会有什么后果？

   **答**：worker 应"持锁取出任务后立即解锁，再执行任务"。若在持锁状态下执行：①整个池被串行化——一个任务跑多久，其他 worker 就取不到任务、生产者也入不了队，并发度退化为 1；②任务里若再 `submit`（同一把锁）会自死锁；③长任务会无限期堵住关闭和入队。所以锁只保护队列这段极短的临界区，任务执行必须在锁外。

4. 怎么优雅关闭线程池？"丢弃剩余任务"和"执行完剩余任务"两种语义如何实现？

   **答**：设 `stop_` 标志，关闭时加锁置位并 `notify_all` 唤醒所有阻塞 worker；worker 取任务的谓词是"队列非空 **或** stop_"。**执行完剩余任务**（优雅 drain）：worker 循环条件为"队列还有任务就继续取，直到队列空且 stop_ 才退出"，析构里 join 所有线程——保证入队任务都跑完。**丢弃剩余任务**：置 stop_ 后直接清空队列再唤醒，worker 见 stop_ 即退出、不再取。通常析构走 drain 语义并 join 等所有 worker 收尾，避免 detach 导致访问已析构的池。

5. `future`、`promise`、`packaged_task`、`async` 四者关系？

   **答**：它们围绕一个**共享状态**（存放结果/异常+就绪标志）协作。`future` 是**读取端**——`get()` 阻塞等结果。`promise` 是**手动写入端**——`set_value/set_exception` 写结果，`get_future()` 拿配对的 future。`packaged_task` 是**包了可调用对象的写入端**——调用它就自动把返回值写进共享状态，等于"promise + 一次函数调用"。`std::async` 是最上层便利封装——给个函数它自动选新线程或延迟执行，直接返回 future，内部就是类似 packaged_task 的机制。关系链：async/packaged_task/promise 负责"写"，future 负责"读"，共享状态是它们之间的管道。

6. 任务队列无上限有什么风险？怎么做背压（有界队列 / 拒绝策略）？

   **答**：无上限队列在"提交速度持续 > 执行速度"时无限堆积→内存暴涨甚至 OOM，故障从局部变成拖垮整个进程，且排队延迟无限拉长。背压手段：①**有界队列**——满时让 `submit` 阻塞，把压力反馈给提交方（自然限速）；②**拒绝策略**——满时直接拒绝（抛异常/返回失败）、丢弃最老任务、或"调用者自己执行"（CallerRuns，借提交线程顺便干活兼限速）；③配合监控/限流。选择取决于任务可丢与否、调用方能否被阻塞。

7. 如何支持"动态扩缩容"线程数？work-stealing（工作窃取）又是什么思路？

   **答**：**扩缩容**：监控队列积压/线程忙闲，积压高于阈值就新建 worker（不超上限），worker 空闲超时（取任务等待超过 keep-alive）就自行退出回收，核心线程保活、临时线程按需增减——类似 Java `ThreadPoolExecutor` 的 core/max/keepAlive。**work-stealing**：不用一个全局队列（它是争用热点），而是**每个 worker 一个本地双端队列**，自己从一端 push/pop（无竞争、缓存热）；本地队列空了就去**偷**别的 worker 队列**另一端**的任务来做。好处是大幅降低中心队列争用、负载自动均衡，是 Fork-Join、Go 调度器、TBB 的核心思路；代价是实现更复杂、需无锁 deque。
