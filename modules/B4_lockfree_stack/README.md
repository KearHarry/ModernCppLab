# B4 · 无锁栈 Treiber Stack 与 ABA 问题（lock-free / CAS）

难度 ⭐⭐⭐⭐ · 预计 3h

## 为什么大厂爱考
"用过无锁吗？""CAS 是什么？""ABA 怎么解决？"——并发岗的分水岭题。会用 mutex 只是入门，能讲清**无锁栈**怎么靠一条 CAS 指令在多线程下不丢数据、能识别出 **ABA** 这种隐蔽 bug，才说明你真的理解了原子操作与内存模型。本模块承接 B3（原子量/内存序），把 CAS 用起来造一个真能跑的并发容器。

## 背景知识

### 1) 为什么要无锁
mutex 正确但有代价：抢不到锁的线程被 OS **挂起/唤醒**（上下文切换贵）；持锁者被抢占时其他线程干等（护航效应、优先级反转）。**无锁(lock-free)** 用原子指令让线程"各自重试、谁也不被无限挂起"，吞吐更稳、更抗尾延迟。代价是难写、易踩 ABA / 内存回收的坑。

### 2) CAS：compare_exchange 的精确语义
```cpp
head_.compare_exchange_weak(expected, desired, success_order, failure_order)
```
**原子地**执行：若 `head_ == expected` 就改成 `desired` 返回 `true`；否则把 `expected` **回写为 `head_` 的当前值**并返回 `false`。那个"失败回写 expected"是写无锁循环的关键——失败后 `expected` 已是最新值，据此重算 `desired` 再试即可。`_weak` 允许偶发"伪失败"，但在循环里更快，所以无锁重试都用 `_weak`。

### 3) Treiber 栈：push / pop 的乐观重试
```
push(x):                          pop():
  n = new Node(x)                   old = head            (acquire)
  n->next = head    (relaxed)       while old && !CAS(head, old, old->next):
  while !CAS(head, n->next, n):         // 失败时 old 已被回写为最新 head
      // 失败时 n->next 已被回写       if !old: return false   // 空栈
  size++                            out = old->value; delete old; size--
```
没有锁，只有"读当前栈顶 → 试着原子替换 → 被抢就按最新值重来"。

### 4) 内存序：release 发布 / acquire 接收（回顾 B3）
push 成功用 **release**：把"我刚写好的节点内容"发布出去；pop 用 **acquire**：摘节点时要能看到 push 写入的内容。release↔acquire 配对形成 happens-before，跨线程才看得到节点数据。链接动作本身不依赖别人数据时（如 push 里读旧 head）可用 `relaxed`。

### 5) ABA 问题（本模块核心考点）
CAS 判断"head 还等于我读到的 old 吗？相等就以为没变"。但**相等 ≠ 没变过**。栈 A→B→C，head==A：
1. 线程1 读 `old=A`，算出 `old->next=B`，准备 `CAS(head, A, B)`，被切走。
2. 线程2 弹出 A、弹出 B，再 push 一个**地址恰好又是 A** 的新节点（分配器复用了释放的地址），令 `A->next=C`，于是 head 又 ==A。
3. 线程1 醒来，`CAS(head, A, B)` 看到 head==A **就成功了**！可 B 早被弹出/释放，head 被设成**悬空指针** → 崩溃/数据损坏。

值 A→B→…→A 绕了一圈，CAS 察觉不到。常见解法：
- **带版本号/标签的指针**（tagged pointer，把 head 做成 `(指针,计数)`，每次改动计数+1，CAS 连计数一起比）。
- **危险指针(hazard pointer) / RCU / epoch**：延迟回收，确保没人还在读才 free，从根上消除"地址复用"。
- 直接用成熟库（folly、boost.lockfree）。

### 6) 本模块的简化边界（重要）
pop 里 `delete old` 在**并发 pop**时不安全（别的线程可能正读 `old->next` → use-after-free，这就是 ABA/回收难题）。彻底解决需危险指针，超出本模块范围。所以**并发测试只做"多线程并发 push + 之后单线程逐个 pop（drain）"**：push 永不 free、drain 时无并发，安全。理解这一点，你就懂了"无锁栈看着简单、工业级实现却很难"。

## 你要实现什么
打开 [include/lockfree_stack.hpp](include/lockfree_stack.hpp)。`Node`、原子 `head_/size_`、析构、`empty()/size()` 已给好。你实现 2 个 `TODO`：

| 编号 | 函数 | 任务 |
|------|------|------|
| B4-1 | `push(value)` | new 节点 + CAS 重试接到栈顶（release），成功后 `size_++` |
| B4-2 | `pop(out)` | CAS 重试摘下栈顶（acquire），取值、`delete`、`size_--`，空栈返回 false |

## 关键坑
- **CAS 失败时 `expected` 已被改写**：循环体可以留空，直接重试；千万别在循环里重新 `head_.load()` 覆盖掉它（多此一举且引入竞态）。
- **`compare_exchange_weak` 必须放在循环里**：它会伪失败，单次调用不可靠。
- **内存序别全写 `seq_cst` 图省事**：理解 release/acquire 为何配对（B3 的内容）才是考点；但写错成全 `relaxed` 会导致读到未初始化的节点内容。
- **`push` 里 `n->next = head_.load()` 用 `relaxed` 即可**：此处只是读个指针值，真正的发布在 CAS 的 release 上。
- **不要在并发 pop 场景里指望本实现安全**：那需要危险指针（导读已说明），测试也特意只在单线程 drain。

## 如何验证
```powershell
ctest --test-dir build -R B4 --output-on-failure
```
测试覆盖：单线程 **LIFO 顺序** + `size` + 空栈 `pop` 返回 `false`；**4 线程并发各 push 1000 个**、join 后单线程 drain，校验**数量与总和都不丢不重**（CAS 重试若漏掉节点，总和对不上）。

> 提示：骨架里 `push` 是空操作、`pop` 恒返回 `false`，所以计数类断言一开始全红；但 `pop` 不解引用、`push` 不分配，join 立即返回，全程不崩溃、不卡死。单线程用例用 `ASSERT_TRUE(s.pop(v))` 先挡住。

## 面试追问

1. CAS 是什么？`compare_exchange_weak` 和 `_strong` 的区别？为什么循环里常用 `_weak`？

   **答**：CAS（compare-and-swap）是原子的"比较并交换"：若内存现值==期望值则写入新值返回 true，否则把现值读回期望变量返回 false。`weak` 允许伪失败（值匹配也可能返回 false），但在 LL/SC 架构上能编成更短的指令；`strong` 不会伪失败。循环里常用 `weak` 是因为外层本就要 `while(!cas) 重试`，伪失败只是多转一圈、代价小，却换来每次迭代更便宜的指令——总体更快。没有循环的一次性 CAS 才用 `strong`。

2. 无锁(lock-free) / 无等待(wait-free) / 无阻塞(obstruction-free) 各是什么含义？Treiber 栈属于哪种？

   **答**：三者是非阻塞进展性的强弱层级。**obstruction-free**（最弱）：某线程单独运行（其他都暂停）能有限步完成，但多个一起跑可能互相干扰永不前进（活锁）。**lock-free**：保证**整个系统**总有**某个**线程能在有限步内前进（不会全体卡死），但个别线程可能一直重试饿死。**wait-free**（最强）：**每个**线程都能有限步完成，无饥饿。Treiber 栈是 **lock-free**：CAS 失败的线程重试，总有一个能成功，但高争用下某个倒霉线程可能反复失败。

3. 讲讲 ABA 问题：怎么发生、后果是什么、有哪些解法（tagged pointer / hazard pointer / RCU）？

   **答**：**发生**：线程 1 读到栈顶 A，准备 `CAS(head, A, A->next)`；切走期间线程 2 把 A pop 掉、又 pop 了 B、再把 A push 回来（A 被释放又重分配到同地址），此时 head 又是 A 但 `A->next` 已不是线程 1 记的那个。线程 1 的 CAS 看到 head 仍是 A→**误判没人动过**→成功，却把 head 设成失效/错误的 next。**后果**：链表结构损坏、丢节点、悬垂指针、崩溃。**解法**：**tagged pointer**（指针+版本号打包，每次改递增 tag，ABA 因 tag 不同被 CAS 拒绝）；**hazard pointer**（线程发布"正在用的指针"，回收者见到就推迟释放，从根上不让 A 被重用）；**RCU/epoch**（延迟回收，确保没有读者还可能引用旧节点后才真正释放）。

4. 无锁栈的 `pop` 为什么不能简单地 `delete` 节点？内存回收为什么是无锁编程最难的部分？

   **答**：`pop` 用 CAS 摘下栈顶后不能立刻 `delete`，因为**别的线程可能正持有指向该节点的指针**（它在自己的 CAS 重试中刚读了 head==这个节点，正要解引用 `node->next`）；你一删，它就解引用悬垂指针→UB。这正是难点：有锁时临界区天然界定了"没人再用"的时刻，无锁里多个线程同时摸同一组节点，无法简单判断"何时再没有任何线程引用某节点"才能安全释放。于是需要 hazard pointer、epoch-based reclamation、RCU 等专门的延迟回收机制——它们是无锁数据结构里最复杂、最易错的部分。

5. push/pop 的内存序为什么用 release/acquire？换成全 `relaxed` 会出什么问题？全 `seq_cst` 又有什么代价？

   **答**：`push` 里"初始化新节点的 next/数据"必须先于"CAS 把它接上 head"对别的线程可见，所以 CAS 用 `release` 发布；`pop` 里 `load(head)`/CAS 用 `acquire`，保证读到栈顶后能看到 push 者写入的节点内容。全 `relaxed`：没有同步关系，pop 线程可能看到 head 已更新、却**看不到该节点的 next/数据初始化**（写被重排或不可见）→读到垃圾、崩溃。全 `seq_cst`：正确，但它要求全局单一全序，多数平台要插更强的内存栅栏（x86 上 store 要 `mfence`/`xchg`），比 acquire/release 慢；而 release/acquire 已足够保证栈正确，是性能与正确性的最佳点。

6. 无锁一定比有锁快吗？什么场景下 mutex 反而更合适？（低争用、临界区大、需要阻塞等待时）

   **答**：不一定。无锁的优势是无阻塞、无优先级反转、无"持锁线程被挂起拖垮全员"，在高争用、临界区极短时通常更好；但它靠 CAS 重试，高争用下重试/缓存弹跳可能很多，且内存回收开销大、代码极难写对。mutex 更合适：**低争用**（基本拿得到锁，连重试都省）、**临界区大/复杂**（无锁几乎无法实现，重试成本随临界区线性放大）、**需要阻塞等待**（等条件满足时让出 CPU 睡眠，比自旋/重试省电省核）、以及对**代码可维护性**有要求时。

7. CAS 在 x86 上对应什么指令（`lock cmpxchg`）？"伪失败"在哪些平台（LL/SC，如 ARM）会发生？

   **答**：x86 上 CAS 编译成带 `lock` 前缀的 `cmpxchg`（如 `lock cmpxchg`），由缓存一致性协议保证原子，**不会伪失败**——所以 x86 上 weak 和 strong 实现相同。伪失败发生在 **LL/SC**（load-linked/store-conditional）架构：ARM（LDREX/STREX、LDXR/STXR）、PowerPC（lwarx/stwcx）、RISC-V（lr/sc）、MIPS。SC 在这些平台上即使值没变，只要中间发生上下文切换、缓存行被其他访问"打断"了链接监视就会失败返回——这就是 weak 允许的伪失败，正好映射 LL/SC 语义，故循环里用 weak 最高效。

8. 怎么把这个栈扩展成无锁队列？为什么队列比栈更难（要同时维护 head 和 tail）？

   **答**：经典做法是 Michael-Scott 无锁队列：带哨兵(dummy)节点的单链表，`head` 指出队端、`tail` 指入队端，enqueue 用 CAS 把新节点接到 `tail->next` 再 CAS 推进 `tail`，dequeue 用 CAS 推进 `head`。比栈难在：栈只有**一个**竞争点（栈顶 head），一次 CAS 搞定；队列有**两个**端点 head/tail 要分别维护，且 enqueue 是"先链接节点、再移动 tail"两步非原子，中途别的线程会看到 tail"落后"一个，必须设计成任何线程都能帮忙把 tail 往前推（helping 机制），还要处理空队列、head 追上 tail 等边界——竞争点翻倍、中间态更多，所以更难写对，ABA 与回收问题也更棘手。
