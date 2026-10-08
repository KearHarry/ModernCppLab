# B7 · C++ 内存模型：Happens-Before、Fence 与引用计数

难度 ⭐⭐⭐⭐ · 预计 4h

## 为什么大厂爱考

原子操作“不会撕裂”并不等于并发程序正确。面试官真正想看的是：你能否画出 **sequenced-before → synchronizes-with → happens-before** 链，判断普通内存访问有没有数据竞争；能否解释 release/acquire 为何发布了旁边的普通数据；能否在引用计数归零时保证析构看见其他线程留下的写。

B3 教你使用 atomic/CAS，本模块专门训练**证明同步协议**。代码很短，但每一个 memory order 都必须有理由。

## 背景知识

### 1. Data race 为什么是 UB

两个线程并发访问同一内存位置，至少一个是写，并且两次访问都不是 atomic，若它们之间没有 happens-before，就是 data race。C++ 将其定义为未定义行为：不只是“可能读到旧值”，编译器可以基于“合法程序没有数据竞争”进行激进优化。

mutex、线程启动/`join`、条件变量、部分 atomic 操作都会建立同步；`volatile` 不会。`volatile` 主要服务内存映射 I/O 和限制特定编译器优化，不提供跨线程原子性或排序。

### 2. 三种顺序关系

- **sequenced-before**：单个线程内部，抽象机规定 A 先于 B。
- **synchronizes-with**：例如 release 写与读到它的 acquire 读之间的跨线程边。
- **happens-before**：由前两者及其传递闭包组成。若写 happens-before 读，读才能安全观察该写。

每个 atomic 对象还有自己的 **modification order**：所有线程对它的修改形成一致的全序，但不同 atomic 之间不自动形成一个总顺序（除非使用更强的 seq_cst 规则）。

### 3. Release / Acquire 发布协议

```cpp
// 线程 A
payload = 42;
ready.store(true, std::memory_order_release);

// 线程 B
if (ready.load(std::memory_order_acquire))
    use(payload);
```

若 acquire 确实读到该 release 写入的值，则 A 对 payload 的普通写 happens-before B 的普通读。只把 ready 改成 atomic 而使用 relaxed 并不足以发布 payload。

### 4. Fence 版本

屏障也能承担排序：发布者先写数据、执行 release fence，再 relaxed-store 标志；消费者先 relaxed-load 并确认标志，再执行 acquire fence，最后读数据。屏障必须通过同一 atomic 标志“搭桥”；在任意位置各放一个 fence 并不会凭空同步。

fence 版本通常比直接给标志使用 release/acquire 更难审阅，只有在需要把一组 relaxed 操作统一排序等场景才值得使用。

### 5. Happens-before 的传递性

线程 A release 发布给 B；B acquire 后再 release 给 C；C acquire 后也能看到 A 的早期写。`HappensBeforeChain` 把这条 A → B → C 的同步链显式化。注意 B 必须真的读到 A 发布的值，C 也必须读到 B 转发的值。

### 6. 引用计数为何“加 relaxed，减 release”

持有有效引用的线程增加计数时，对象生命周期已经由旧引用保证，`fetch_add(relaxed)` 只需不丢计数。释放引用需要把该持有者此前的写发布出去；最后归零的线程在析构前还要获取所有持有者发布的结果。

常见优化是：

```cpp
if (refs.fetch_sub(1, std::memory_order_release) == 1) {
    std::atomic_thread_fence(std::memory_order_acquire);
    delete object;
}
```

这比每次减计数都用 acq_rel 更弱：只有真正归零者支付 acquire 成本。它依赖该原子对象上的 release sequence。引用计数只保护**对象寿命**，不会自动让对象成员的并发读写安全。

### 7. 为什么测试不能证明内存序正确

错误的 relaxed 程序可能在 x86、Debug 构建和一百万次运行中“都正常”，却在 ARM、优化构建或新编译器上失败。测试只能发现部分错误，不能穷举标准允许的执行。正确流程是：先以标准关系证明，再用压力测试、ThreadSanitizer、模型检查器或专用 litmus 工具辅助验证。

本模块的测试故意不制造 UB，也不依赖偶现重排；它验证封装的状态机与结果，memory order 的正确性还需你对照协议审阅。

为避免“代码跑绿就当作内存序正确”，头文件把各操作应使用的顺序集中在 `memory_order_policy` 编译期常量中，测试用 `static_assert` 锁定这些策略，TODO 示例也必须引用同一组名字。静态断言只能证明代码声明了预期策略；仍需结合操作位置和 read-from 关系完成人工证明。

## 你要实现什么

打开 [include/memory_model.hpp](include/memory_model.hpp)：

| 编号 | API | 任务 |
|---|---|---|
| B7-1 | `ReleaseAcquireMailbox::publish` | 普通写后 release 发布标志 |
| B7-2 | `try_consume` | acquire 确认标志后读取普通数据 |
| B7-3 | `FenceMailbox::publish` | release fence + relaxed 标志写 |
| B7-4 | `FenceMailbox::try_consume` | relaxed 标志读 + acquire fence |
| B7-5 | `HappensBeforeChain` 三个方法 | 建立 A → B → C 的传递同步链 |
| B7-6 | `AtomicRefCount::add_ref` | relaxed 增加引用 |
| B7-7 | `AtomicRefCount::release_ref` | release 减引用，归零后 acquire fence |

## 关键坑

- “atomic 标志是安全的”不代表被它保护的普通数据自动安全；要证明 synchronizes-with。
- acquire 只有在读到 release（或其 release sequence）产生的值时才同步。
- CAS 的失败序不能是 release/acq_rel，也不能强于成功序；失败路径本质上只是一次 load。
- release fence 应在标志写前，acquire fence 应在确认标志后；位置错了协议就断。
- relaxed 计数适用于统计或已由别处保护的生命周期，不适合拿来随意发布对象。
- 引用计数归零后不得再从裸指针“复活”对象；这需要 weak count 或其他外部协议。
- `is_lock_free()` 是类型、平台甚至地址相关的实现属性；“用了 atomic”不等于算法 lock-free。

## 如何验证

```powershell
ctest --test-dir build -R B7 --output-on-failure
```

测试覆盖策略常量的编译期检查、未发布读取、release/acquire 邮箱、fence 邮箱、传递同步链、单线程引用状态机和多线程 retain/release。运行测试通过只说明这些确定性功能路径得到预期结果，**不能证明普通 payload 在所有编译器与硬件上都由所写内存序正确发布、可见**；例如线程 `join` 或同线程调用本身也可能让错误实现看起来正常。建议额外用 Clang/GCC 的 ThreadSanitizer 构建；但 sanitizer 通过同样不替代内存模型证明。

## 面试追问

1. **原子性、可见性、有序性分别是什么？**

   **答：**原子性保证某操作不可被观察成中间状态；可见性描述一个线程的写何时能被另一个看到；有序性约束编译器/CPU 可观察的重排。atomic 默认 seq_cst 同时给出较强排序，但 relaxed 主要只保本原子对象的原子性与 modification order。

2. **release/acquire 何时真正配对？**

   **答：**acquire load 必须从对应 release store 或其 release sequence 读到值，才产生 synchronizes-with。只是对同一变量分别用了 release/acquire、但 acquire 读到更早的值，不会同步。

3. **`memory_order_seq_cst` 是否意味着程序一定没有数据竞争？**

   **答：**否。它只约束使用它的原子操作并建立 seq_cst 总序；对未同步的普通变量并发读写仍是 data race。内存序不能弥补遗漏的共享数据协议。

4. **compiler fence 与 atomic thread fence 有何区别？**

   **答：**`atomic_signal_fence` 主要约束编译器与信号处理相关的重排，通常不发 CPU 屏障；`atomic_thread_fence` 参与线程间内存模型并可能生成硬件屏障。前者不能替代跨核同步。

5. **为什么引用计数不能解决对象内容的线程安全？**

   **答：**计数只保证“至少一个引用存在时对象不销毁”。两个持有者仍可能同时写同一成员而数据竞争；成员访问仍需 mutex、atomic 或不可变设计。

6. **release sequence 是什么，为什么引用计数关心它？**

   **答：**它从一次 release 修改开始，沿该 atomic 对象后续相关的 read-modify-write 修改延伸。最后归零的 RMW/获取屏障可以汇合此前释放者发布的写，使析构线程看到对象生命周期末期的状态。

7. **`memory_order_consume` 为什么几乎不用？**

   **答：**它只沿数据依赖排序，理论上比 acquire 便宜，但依赖传播在优化器里极难可靠实现；主流编译器长期把它提升为 acquire。工程里通常直接使用 acquire，标准也在持续重做 consume 语义。

8. **如何评审一个无锁协议？**

   **答：**列出共享对象与每次访问；为每个普通冲突访问找到 happens-before；标注 atomic 的 modification order 与每次 read-from；检查失败/CAS/回收路径；再检查进度保证和 ABA。最后才用 sanitizer、压力测试和弱内存模型工具辅助。
