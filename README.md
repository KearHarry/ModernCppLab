# ModernCppLab · C++ 大厂面试特训营

> 一个 **MIT 课程作业式** 的现代 C++ 自学框架。每个模块给你一套带 `TODO` 的代码骨架和一组测试用例——你读知识点讲解、亲手填空实现、跑测试验证。在「红变绿」的过程中，把腾讯 / 字节 / 米哈游等大厂的高频考点真正吃透。

本仓库**不教语法基础**（变量、循环、`cout` 那些）。它聚焦四件事：

- **现代 C++ 特性**（C++11 → C++20）：移动语义、智能指针、模板元编程、类型擦除、`optional`、协程……
- **并发编程**：读写锁、条件变量、原子与内存序、无锁结构、线程池……
- **STL 底层实现**：手写 `vector` / `string(SSO)` / `unordered_map` / 跳表 / 侵入式链表……
- **经典工程题与对象模型 + 引擎设施**：LRU、内存池、单例、vtable、Pimpl、Slot Map、帧分配器、信号-槽……

> 共 **5 个轨道 · 26 个模块**，每个模块都配有详尽的知识点讲解、带原理注释的 `TODO` 骨架、测试用例，以及一组带**参考答案**的「面试追问」。

---

## 学习方法（怎么用这个仓库）

每个模块就是一次「实验作业 (lab)」，标准流程：

1. **读模块的 `README.md`** —— 知识点背景、为什么大厂爱考、要实现什么、有哪些坑。
2. **读源码里的 `TODO`** —— 每个 `TODO` 都详细描述了「要做什么 + 背后的原理 + 关键步骤」，读完应当能独立写出实现。
3. **动手实现** —— 把 `TODO` 处的桩代码替换成你的实现。
4. **跑测试** —— `ctest -R <模块号>`，看红色断言一个个变绿。
5. **答追问** —— 每个模块 `README` 末尾有「面试追问 + 参考答案」，先自己讲一遍，再对答案查漏补缺。

> 骨架代码**开箱即能编译**（桩函数返回占位值），所以你一开始就能跑测试看到「全红」，然后逐步实现、逐步变绿，不会卡在编译错误上。

---

## 环境与构建

需要：支持 C++20 的编译器（g++ ≥ 11 / clang ≥ 14 / MSVC 2022）、CMake ≥ 3.20。
本仓库在 **MinGW-w64 g++ 14.1 + CMake 3.30** 上验证通过。

```powershell
# 1) 生成构建系统（Windows + MinGW）
cmake -S . -B build -G "MinGW Makefiles"

# 2) 编译全部模块
cmake --build build -j

# 3) 运行全部测试
ctest --test-dir build --output-on-failure

# 只跑某个模块（按测试名前缀过滤，例如 A1 / B5 / E2）
ctest --test-dir build -R A1 --output-on-failure

# 也可直接运行单个测试可执行文件，输出更详细：
./build/modules/A1_move_semantics/A1_move_semantics.exe   # Windows
./build/modules/A1_move_semantics/A1_move_semantics       # Linux/Mac
```

> 懒人脚本：`scripts/build.ps1`（Windows）或 `scripts/build.sh`（Linux/Mac），一键配置 + 编译 + 测试。

用其它生成器（Ninja / Visual Studio）只需替换 `-G`：

```powershell
cmake -S . -B build -G "Ninja"
```

---

## 课程地图

> 难度：⭐ 入门 / ⭐⭐ 需动脑 / ⭐⭐⭐ 高强度 / ⭐⭐⭐⭐ 硬核 / ⭐⭐⭐⭐⭐ 地狱。
> 建议按轨道内顺序推进；轨道之间可并行。

### A 轨 · 现代 C++ 核心特性
| 模块 | 主题 | 难度 | 核心考点 |
|------|------|------|----------|
| [A1](modules/A1_move_semantics) | 移动语义与完美转发 | ⭐⭐ | 右值引用、`std::move`/`std::forward`、引用折叠、Rule of Five、移动 vs 拷贝 |
| [A2](modules/A2_smart_pointers) | 智能指针 | ⭐⭐⭐ | 手写 `unique_ptr`/`shared_ptr`/`weak_ptr`、控制块、原子引用计数、循环引用 |
| [A3](modules/A3_template_metaprogramming) | 模板元编程 | ⭐⭐⭐ | `type_traits`、SFINAE/`enable_if`、可变参模板、折叠表达式、concepts |
| [A4](modules/A4_type_erasure) | 类型擦除 / 手写 `std::function` | ⭐⭐⭐ | 类型擦除、小对象优化(SBO)、虚表 vs 函数指针、统一存储任意可调用物 |
| [A5](modules/A5_optional) | 手写 `Optional<T>` | ⭐⭐⭐ | 手动生命周期、placement new、aligned storage、显式析构、值语义 |
| [A6](modules/A6_coroutine_generator) | C++20 协程 `Generator<T>` | ⭐⭐⭐⭐ | `coroutine_handle`、`promise_type`、`co_yield`、惰性求值、挂起/恢复 |

### B 轨 · 并发编程
| 模块 | 主题 | 难度 | 核心考点 |
|------|------|------|----------|
| [B1](modules/B1_rwlock) | 读写锁 RWLock | ⭐⭐⭐⭐ | `shared_mutex` 原理、读者/写者优先、避免写饥饿、`condition_variable` |
| [B2](modules/B2_blocking_queue) | 阻塞队列（生产者-消费者） | ⭐⭐ | `mutex`+`condition_variable`、谓词等待、虚假唤醒、优雅关闭 |
| [B3](modules/B3_atomics_memory_order) | 原子操作与内存序 | ⭐⭐⭐ | `atomic`、CAS、自旋锁、`memory_order` acquire/release、SPSC 环形缓冲 |
| [B4](modules/B4_lockfree_stack) | 无锁栈 Treiber Stack | ⭐⭐⭐⭐ | CAS 循环、ABA 问题、tagged pointer、无锁内存回收难题 |
| [B5](modules/B5_thread_pool) | 线程池 | ⭐⭐⭐ | 任务队列、`future`/`packaged_task`、`submit` 泛型接口、优雅停机 |

### C 轨 · STL 底层实现
| 模块 | 主题 | 难度 | 核心考点 |
|------|------|------|----------|
| [C1](modules/C1_vector) | 手写 `vector` | ⭐⭐⭐ | 扩容策略、`size`/`capacity`、移动重分配、异常安全、迭代器 |
| [C2](modules/C2_string_sso) | 手写 `string`（SSO） | ⭐⭐⭐ | 小字符串优化、`union` 布局、容量增长、移动语义 |
| [C3](modules/C3_memory_pool) | 定长对象内存池 | ⭐⭐⭐ | free-list、O(1) 分配/回收、对象复用、碎片与对齐 |
| [C4](modules/C4_hash_table) | 手写 `unordered_map` | ⭐⭐⭐⭐ | 链地址法、负载因子、rehash、冲突、迭代器失效 |
| [C5](modules/C5_intrusive_list) | 侵入式双向链表 | ⭐⭐⭐⭐ | 侵入式节点、零额外分配、O(1) 删除、hook、生命周期 |
| [C6](modules/C6_skip_list) | 跳表 SkipList | ⭐⭐⭐⭐⭐ | 概率平衡、多层索引、O(log n) 查找、范围查询（Redis zset） |

### D 轨 · 经典工程题与对象模型
| 模块 | 主题 | 难度 | 核心考点 |
|------|------|------|----------|
| [D1](modules/D1_lru_cache) | LRU 缓存 | ⭐⭐⭐⭐ | 哈希表 + 双向链表、O(1) get/put、容量淘汰、`splice` |
| [D2](modules/D2_crtp) | CRTP 静态多态 | ⭐⭐⭐ | 编译期多态、零虚表开销、奇异递归模板、mixin |
| [D3](modules/D3_singleton) | 线程安全单例 | ⭐⭐⭐ | Meyers 单例、`call_once`、双检锁陷阱、静态初始化顺序 |
| [D4](modules/D4_object_model) | 虚函数与对象模型 | ⭐⭐⭐ | vtable/vptr、虚析构、`clone`/原型模式、RTTI、对象切片 |
| [D5](modules/D5_pimpl) | Pimpl 编译防火墙 | ⭐⭐⭐ | 不完整类型、编译解耦、ABI 稳定、析构/移动的定义点 |

### E 轨 · 引擎 / 游戏常用设施
| 模块 | 主题 | 难度 | 核心考点 |
|------|------|------|----------|
| [E1](modules/E1_slot_map) | 生成式句柄池 Slot Map | ⭐⭐⭐⭐ | `index+generation` 句柄、O(1) 增删查、防悬垂、ECS 引用底座 |
| [E2](modules/E2_arena_allocator) | 线性 / 竞技场 / 帧分配器 | ⭐⭐⭐ | bump 指针、按帧 `reset`、`marker`/`rewind`、对齐、数据导向设计 |
| [E3](modules/E3_delegate) | 多播委托 / 信号-槽 | ⭐⭐⭐ | 事件解耦、`std::function` 槽、连接凭证、观察者模式工业形态 |
| [E4](modules/E4_string_id) | 编译期字符串哈希 ID | ⭐⭐⭐ | FNV-1a、`constexpr`、UDL `operator""_id`、当 `switch` 标签 |

---

## 进度清单

实现完一个模块就把 `[ ]` 改成 `[x]`：

**A 轨 · 现代 C++ 核心特性**
- [ ] A1 移动语义与完美转发
- [ ] A2 智能指针
- [ ] A3 模板元编程
- [ ] A4 类型擦除 / std::function
- [ ] A5 Optional&lt;T&gt;
- [ ] A6 C++20 协程 Generator

**B 轨 · 并发编程**
- [ ] B1 读写锁 RWLock
- [ ] B2 阻塞队列
- [ ] B3 原子操作与内存序
- [ ] B4 无锁栈 Treiber Stack
- [ ] B5 线程池

**C 轨 · STL 底层实现**
- [ ] C1 手写 vector
- [ ] C2 手写 string (SSO)
- [ ] C3 定长内存池
- [ ] C4 手写 unordered_map
- [ ] C5 侵入式链表
- [ ] C6 跳表 SkipList

**D 轨 · 经典工程题与对象模型**
- [ ] D1 LRU 缓存
- [ ] D2 CRTP 静态多态
- [ ] D3 线程安全单例
- [ ] D4 虚函数与对象模型
- [ ] D5 Pimpl 编译防火墙

**E 轨 · 引擎 / 游戏常用设施**
- [ ] E1 生成式句柄池 Slot Map
- [ ] E2 线性 / 帧分配器
- [ ] E3 多播委托 / 信号-槽
- [ ] E4 编译期字符串哈希 ID

---

## 目录结构

```
ModernCppLab/
├── README.md                 # 你正在读的这个（课程总览）
├── CMakeLists.txt            # 顶层构建（含 add_module_test 辅助函数 + 模块清单）
├── common/
│   └── test_framework.hpp    # 零依赖、header-only 的极简测试框架
├── docs/
│   └── interview_index.md    # 考点 → 模块 的速查索引（面试前复盘用）
├── scripts/
│   ├── build.ps1             # Windows 一键构建 + 测试
│   └── build.sh              # Linux/Mac 一键构建 + 测试
└── modules/
    └── <模块名>/
        ├── README.md         # 知识点讲解 + 任务说明 + 面试追问（含参考答案）
        ├── include/*.hpp     # 带 TODO 的代码骨架（你在这里实现）
        ├── src/*.cpp         # （部分模块有）非模板实现文件
        ├── tests/*.cpp       # 测试用例（你的实现要让它们全部通过）
        └── CMakeLists.txt    # 模块构建（一行 add_module_test）
```

---

## 关于「面试追问」

每个模块 README 末尾都有 8 道**面试追问**，并附**参考答案**——覆盖原理、取舍、翻车场景与扩展方向。建议先盖住答案自己讲，再对照查漏。这部分是把「会写」升级为「讲得清」的关键，也是面试现场真正拉开差距的地方。

---

> 想看某个模块的**参考实现**？框架以「详尽 TODO + 测试」驱动自学，仓库默认不含答案代码，以免剧透。需要时可单独索取对照。

---

## 许可协议

本仓库采用 **[CC BY-NC 4.0](LICENSE)**（知识共享 署名—非商业性使用 4.0 国际）授权：

- ✅ 可自由**下载学习、修改、二次创作、分发**；
- ✍️ 转载 / 分发须**注明出处**（ModernCppLab，作者 KearHarry，及本仓库地址）；
- 🚫 **不得用于商业目的**（如需商用请联系作者获取授权）。

详见 [LICENSE](LICENSE)。
