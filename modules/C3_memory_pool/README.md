# C3 · 定长对象内存池 / free-list 分配器（memory pool）

难度 ⭐⭐⭐ · 预计 2.5h

## 为什么大厂爱考
"频繁 new/delete 小对象为什么慢？怎么优化？"——"手写一个内存池/对象池"是高性能服务器、游戏、数据库岗的高频题。它考的不是 API，而是你**懂不懂内存分配的代价、会不会用空闲链表把分配/释放压到 O(1)、理不理解对齐与 placement new**。本模块也是 STL `allocator` 的微缩原型。

## 背景知识

### 1) 通用 new/delete 为什么慢
`malloc/free` 要应付**任意大小**：维护复杂空闲结构、对抗碎片、多线程下还要加锁。若程序疯狂创建/销毁**同一种小对象**（网络包、链表节点、粒子……），每次都走通用分配器很浪费。内存池抓住"**大小固定**"这一前提把问题做到极简。

### 2) 侵入式空闲链表（intrusive free list）
核心技巧：**空闲块借自己的内存存"下一个空闲块"的指针**。块空闲时内容反正没用，正好拿前 8 字节当 `next`，把所有空闲块串成单链表——**不需要额外管理数组**，元数据寄生在块自身（所以块大小必须 ≥ 一个指针）。
```
free_list_ ─▶ [blk] ─▶ [blk] ─▶ [blk] ─▶ null
               ↑ 每个空闲块前 8 字节 = 指向下一空闲块的指针

allocate():    p = free_list_; free_list_ = p->next; return p;   // 摘头  O(1)
deallocate(p): p->next = free_list_; free_list_ = p;             // 插头  O(1)，后进先出
```

### 3) 按 chunk 批发、按需扩容
池起初为空。第一次分配发现链表空 → `grow_()`：向系统要一整片 `block_size × blocks_per_chunk` 的 **chunk**，切成若干块全挂上 free_list。chunk 用 `unique_ptr<byte[]>` 持有、存进 `chunks_`，**池析构时整片释放**（不逐块 delete）。批发摊薄了系统调用，也减少碎片。

### 4) 对齐（alignment）
返回的内存要满足对齐，否则放 `double`/指针可能变慢甚至崩。本模块构造时把块大小**向上取整**到 `alignof(std::max_align_t)`，而 `new byte[]` 的 chunk 起始地址本身最大对齐 → 每块起点都对齐。（这步已给好，你不用操心。）

### 5) 只给生内存，对象生命周期靠 placement new
`allocate()` 只返回**未构造的生内存**。要造对象用 **placement new**（见 C1）：
```cpp
Widget* w = new (pool.allocate()) Widget{7, 2.5};  // 在池内存上构造
// ... 用 w ...
w->~Widget();             // 先手动析构
pool.deallocate(w);       // 再归还内存
```
给本池套上 `allocate/deallocate` 接口，就能当 `std::vector`/`std::list` 的自定义 allocator——这正是 STL allocator 的工作方式。

## 你要实现什么
打开 [include/memory_pool.hpp](include/memory_pool.hpp)。构造函数（含对齐处理）、`chunks_` 存储、各计数器、观测接口都已给好。你实现 3 个 `TODO`：

| 编号 | 函数 | 任务 |
|------|------|------|
| C3-1 | `allocate()` | 链表空则 `grow_()`，摘下表头返回；更新计数 |
| C3-2 | `deallocate(p)` | 把块头插回空闲链表；更新计数 |
| C3-3 | `grow_()` | 批发一片 chunk，切块全部挂进 free_list |

## 关键坑
- **块大小必须 ≥ 指针大小**：空闲块要存 `next`。构造函数已用 `max(block_size, sizeof(FreeNode))` 兜底，理解为什么。
- **`reinterpret_cast` 切块**：`base + i * block_size_` 定位第 i 块，转成 `FreeNode*` 写 `next`。注意步长是**对齐后的 `block_size_`**，不是原始请求大小。
- **归还不还给系统**：`deallocate` 只是把块挂回链表，内存仍归池所有；真正释放发生在池析构（`chunks_` 自动释放）。
- **分配出的是生内存**：必须 placement new 构造、显式 `~T()` 析构，别直接 `delete` 池给的指针。
- **本池是单线程版**：多线程需加锁或做成无锁 free list（可结合 B4 思考），否则 `free_list_` 会被并发破坏。

## 如何验证
```powershell
ctest --test-dir build -R C3 --output-on-failure
```
测试覆盖：分配出**互不相同、可用**的块并在其上 placement new 构造对象；**归还的块被下次分配立即复用**（验证空闲链表）；**按 chunk 扩容**（每片 4 块，第 5 次分配批发第 2 片）；`outstanding`/`free_count`/`chunk_count` 计数正确。

> 提示：骨架里 `allocate` 恒返回 `nullptr`、`deallocate/grow_` 为空，所有用例都先 `ASSERT_NE(p, nullptr)` 挡在解引用之前，所以一开始只变红、不崩溃。

## 面试追问

1. 内存池为什么比通用 `new/delete` 快？分别快在分配、释放、缓存局部性的哪一环？

   **答**：通用 `new/delete` 要处理**任意大小**，内部维护复杂的空闲块结构、做查找/分裂/合并、还要应对多线程加锁，路径长。定长池快在三环：**分配**——空闲块串成链表，分配就是"摘下链表头"，O(1) 无查找；**释放**——"把块插回链表头"，O(1) 无合并；**缓存局部性**——所有块来自连续的大 chunk、大小一致紧凑排列，顺序访问命中率高、碎片少。代价是只能分固定大小。

2. 侵入式空闲链表怎么做到不需要额外管理结构？为什么块大小必须 ≥ 一个指针？

   **答**：空闲块此刻**没装对象、内容随便用**，于是把"指向下一个空闲块的指针"**直接写在块自身的内存里**，所有空闲块借自己的身体串成一条链——无需额外数组/节点记录谁空闲，这就是"侵入式"。正因为要在块里塞一个指针，**块大小必须 ≥ sizeof(指针)**（通常还要满足指针对齐），否则存不下 next。块被分配出去后这块内存改存对象，回收时再写回 next，复用同一空间。

3. 为什么按 chunk 批发而不是一块块向系统要？这和 `std::deque` 的分段、`std::vector` 的几何扩容有何相似？

   **答**：每次只向系统要一块会触发大量 `malloc`/`mmap` 系统调用（慢、有每次分配的元数据开销、易碎片）。按 chunk **批发**——一次要一大片、切成许多定长块挂进空闲链，把系统调用次数从 O(n) 降到 O(n/块数)、分摊开销。这与 `std::vector` 的**几何扩容**（一次多要、摊还分配成本）、`std::deque` 的**分段数组**（按固定大小的段批量分配、避免整体搬迁）同理：都是"批量预分配 + 摊还"来降低频繁向系统申请的代价。

4. 对齐是什么？不对齐会怎样？`alignof(std::max_align_t)` / `std::aligned_alloc` / `alignas` 各是什么？

   **答**：**对齐**指对象地址必须是其对齐要求的整数倍，硬件按对齐边界高效访问。不对齐在 x86 上变慢、在 ARM 等平台可能直接**总线错误崩溃**，原子操作不对齐还会失去原子性。`alignof(std::max_align_t)` 是平台上"最严格的基本对齐"（一般 16），`malloc` 返回的内存至少满足它；`std::aligned_alloc(align,size)` 申请指定对齐的内存；`alignas(N)` 是声明说明符，强制某变量/类型按 N 字节对齐。内存池切块时必须保证每块起始地址满足目标类型对齐。

5. 这个池怎么改造成 STL allocator（`allocate/deallocate/rebind`）喂给 `std::list`？

   **答**：包一层符合 Allocator 概念的类型：提供 `using value_type=T;`、`T* allocate(n)`、`void deallocate(T*,n)`，以及让容器能"换型"的 `rebind`（C++11 后多由 `std::allocator_traits` 从 `Alloc<T>` 自动推出 `Alloc<U>`，自己提供模板转换构造即可）。关键点：`std::list` 不分配 `T`，而是分配**内部节点 `__list_node<T>`**，所以池的块大小要按**节点大小**（含前后指针+T）而非 `sizeof(T)` 来定——这正是 rebind 的意义：容器用 `Alloc<Node>` 而非 `Alloc<T>`。把它作为 `std::list<T, MyAlloc<T>>` 的第二模板参数即可。

6. 多线程下这个池有什么问题？怎么改（每线程一个池 / 加锁 / 无锁 free list / tcmalloc 的 thread cache 思路）？

   **答**：问题是空闲链表是**共享可变状态**，多线程同时 `allocate`/`deallocate` 改链表头会数据竞争→链表损坏、重复发块。改法由轻到重：**加锁**（简单但成争用瓶颈）；**无锁 free list**（CAS 改表头，但要处理 ABA）；**每线程一个池**（thread-local，分配几乎无争用，但跨线程释放要处理"还给原池"）；**tcmalloc/jemalloc 的 thread cache 思路**——每线程本地缓存一批块（无锁快路径），本地耗尽/积压时再与中央堆批量交换（少量加锁），兼顾低争用与内存平衡，是工业级方案。

7. 定长池 vs 变长分配器（buddy system、slab、伙伴算法、jemalloc/tcmalloc）的取舍？

   **答**：**定长池**只管一种大小，分配/释放 O(1)、无碎片、实现极简，但只适合大量同型对象。**变长分配器**要应对任意大小：**slab**——按常见对象大小分多个定长池（slab class），兼顾通用与快速，Linux 内核用；**buddy（伙伴）系统**——按 2 的幂分块、释放时与"伙伴"合并，碎片可控、支持大范围尺寸，但有内部碎片（向上取整到 2 的幂）；**jemalloc/tcmalloc**——分级 size class + 线程缓存 + 中央堆，综合性能和多线程扩展性最好，但复杂。取舍：尺寸单一、追求极致用定长池；尺寸多样、要通用就用 slab/伙伴或成熟 malloc 实现。

8. 内存池如何与 placement new / 显式析构配合管理对象生命周期？只回收内存不析构对象会怎样？

   **答**：内存池只负责**原始内存**的发放与回收，对象的**构造/析构**由使用者配合：拿到块后用 **placement new** `::new(p) T(args)` 在上面构造，归还前必须**显式析构** `p->~T()`，再把内存还给池。若**只回收内存不析构对象**，T 持有的资源（堆内存、文件句柄、锁）不会被释放→**资源泄漏**，且若该内存被复用、在未析构的对象上又 placement new，旧对象的析构永远不发生、语义错乱。所以"池管内存、使用者管对象生命周期"必须成对：construct↔destroy、allocate↔deallocate。
