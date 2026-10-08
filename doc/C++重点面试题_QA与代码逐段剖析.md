# C++ 重点面试题：Q&A、代码与执行过程剖析

> 更新：2026-09-22。以 C++20 为统一编译基线；属于 C++17/20 的关键行为会单独标明。
>
> 这份文档适合在读完概念后练习“看代码解释结果”和“当场手写”。沿着 **类型与生命周期 → 资源和所有权 → 多态与泛型 → 容器与算法 → 并发与内存模型** 逐步追问。题目是教学整理，不冒充某家公司当年的原题。
>
> 每个 `cpp` 代码块都是包含头文件和 `main()` 的独立程序，不能把所有块直接拼成一个文件。断言用于检查文中结论；错误写法只放在注释或正文中，不执行未定义行为。

## 阅读路线

| 追问链 | 题目 | 练习目标 |
| --- | --- | --- |
| 类型如何决定使用方式 | [01 类型推导](#q01) → [02 生命周期](#q02) → [03 内存与构造](#q03) | 区分值、引用、地址、存储和对象 |
| 对象怎样复制与转移 | [04 五法则](#q04) → [05 完美转发](#q05) → [06 返回值优化](#q06) | 能解释每一次构造与资源转移 |
| 所有权怎样约束回调 | [07 独占指针](#q07) → [08 共享和弱引用](#q08) → [09 异步回调](#q09) | 手推对象何时销毁 |
| 统一接口如何工作 | [10 虚函数](#q10) → [11 类型转换](#q11) → [12 模板约束](#q12) → [13 function 与 variant](#q13) | 分清静态类型、动态类型、编译期与运行期 |
| 容器为何会失效 | [14 扩容与异常](#q14) → [15 删除与失效](#q15) → [16 有序和哈希容器](#q16) | 说清复杂度、失效范围与异常边界 |
| 用容器解决问题 | [17 二分与比较器](#q17) → [18 LRU](#q18) | 写不变量，验证边界与多结构一致性 |
| 共享状态如何同步 | [19 锁与死锁](#q19) → [20 阻塞队列](#q20) → [21 任务与异常](#q21) | 让等待、退出、异常都有明确路径 |
| 从同步到可见性 | [22 发布数据](#q22) → [23 CAS](#q23) → [24 单例](#q24) | 能证明同步关系，而非依赖测试碰巧通过 |
| 面试进阶手写 | [25 内存重叠拷贝](#q25) → [26 第 K 大](#q26) | 处理重叠、重复值、边界和复杂度 |

每题建议这样练：先读问题 → 预测断言结果 → 自己写关键代码 → 对照执行过程 → 回答追问。P0 表示本讲义建议优先掌握，P1 为进阶；不是公司出题概率统计。

---

<a id="q01"></a>
## 01｜P0：`auto`、引用和 `const` 到底推导出什么？

**面试问题：** 同一个变量经过 `auto`、`auto&`、`decltype` 推导，为什么得到不同类型？指针本身为 const 和对象为 const 有什么区别？

**核心回答：** 先区分“声明的类型”和“表达式的值类别”。普通按值 `auto` 通常去掉引用和顶层 const；`auto&` 保留引用及所引用对象的 const。`decltype(名字)` 对未加括号的名字取声明类型；`decltype((表达式))` 根据值类别推导，左值产生 `T&`。多一对括号可能改变返回类型。

**可运行代码：**

```cpp
#include <cassert>
#include <type_traits>

int main() {
    const int value = 7;
    auto a = value;
    auto& b = value;
    decltype(value) c = 9;
    decltype((value)) d = value;
    static_assert(std::is_same_v<decltype(a), int>);
    static_assert(std::is_same_v<decltype(b), const int&>);
    static_assert(std::is_same_v<decltype(c), const int>);
    static_assert(std::is_same_v<decltype(d), const int&>);

    int x = 1, y = 2;
    int* const fixed = &x;
    const int* observer = &x;
    *fixed = 10;
    observer = &y;
    assert(x == 10 && *observer == 2);
    // fixed = &y;  // 编译错误：指针本身不可改。
    // *observer = 3; // 编译错误：不能经此指针修改对象。
}
```

**逐段剖析：**

1. `a` 是独立的整数副本，修改它不影响 `value`；`b` 与 `d` 则引用原对象。
2. `fixed` 的地址不可换，但可以修改它指向的非 const 整数；`observer` 恰好相反。
3. `const int*` 并不保证对象永远不变，只限制经该指针的写入；对象可能被其他合法路径修改。

**易错点：** `const shared_ptr<T>` 限制包装器重绑，不把 T 变成 const。若要表达只读对象，要考虑 `shared_ptr<const T>`。

**顺势追问：** 引用没有复制对象，那它引用的对象先销毁了怎么办？进入第 02 题。

---

<a id="q02"></a>
## 02｜P0：返回引用、`string_view` 和 lambda 为什么会悬垂？

**面试问题：** `const&` 能延长临时对象生命周期，为什么返回引用仍会出问题？`string_view` 是否比 `string` 更安全？

**核心回答：** 生命周期延长只适用于特定直接绑定情形，不能靠“把引用再传一层”无限延长。视图、引用、裸指针都不拥有底层对象；必须说明底层对象活多久以及是否会被修改到使地址失效。

**可运行代码：**

```cpp
#include <cassert>
#include <string>
#include <string_view>

std::string make_name() { return "player"; }

int main() {
    const std::string& extended = make_name();
    assert(extended == "player");

    std::string owner = make_name();
    std::string_view view = owner;
    assert(view.substr(0, 4) == "play");

    // std::string_view bad = make_name();
    // 上一行分号后临时 string 已销毁，不能再读取 bad。

    auto safe_callback = [copy = owner] { return copy.size(); };
    owner.clear();
    assert(safe_callback() == 6);
    // view 此时不能再被当作原来的 6 个有效字符使用。
}
```

**执行过程：** `extended` 直接绑定返回的临时 string，该临时对象活到该局部引用作用域结束。`view` 只记录地址和长度；清空 owner 不会自动把 view 的长度同步为 0。lambda 则自己保存一个 string 副本，因此不依赖 owner 的后续状态。

**追问一：返回 `const T&` 一定错误吗？** 不一定。可以返回调用方传入的长寿命对象或成员，但必须说明被引用对象的有效期。返回本函数局部对象的引用则错误。

**追问二：`decltype(auto)` 有什么坑？** `return local;` 与 `return (local);` 可能分别推导成值与引用，后者返回局部变量引用会悬垂。类型推导不负责延长对象寿命。

规则校核：[C++ 草案：临时对象与生命周期延长](https://eel.is/c++draft/class.temporary)。

---

<a id="q03"></a>
## 03｜P0：申请了内存，为什么还必须构造对象？

**面试问题：** 区分 `new` 表达式、`operator new`、placement new；容器为何把 allocate 与 construct 分开？

**核心回答：** `new T(args)` 通常包含取得存储和初始化对象；单独 `operator new`/allocator 主要处理存储。手写容器必须维护“哪些槽位已有活着的对象”，不能把 capacity 个槽位全部当作 size 个对象访问。下面以需要构造/析构的类型演示；C++20 对部分隐式生命周期类型另有规则，不能简单概括成所有原始分配都绝不开始任何对象生命周期。

**可运行代码：**

```cpp
#include <cassert>
#include <memory>
#include <stdexcept>

struct Item {
    static inline int alive = 0;
    explicit Item(bool fail) {
        if (fail) throw std::runtime_error("construction failed");
        ++alive;
    }
    ~Item() { --alive; }
};

void create_once(bool fail) {
    std::allocator<Item> allocator;
    Item* storage = allocator.allocate(1);
    try {
        std::construct_at(storage, fail);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    assert(Item::alive == 1);
    std::destroy_at(storage);
    allocator.deallocate(storage, 1);
}

int main() {
    create_once(false);
    try { create_once(true); }
    catch (const std::runtime_error&) {}
    assert(Item::alive == 0);
}
```

**逐段剖析：**

1. `allocate(1)` 提供适合 Item 的存储和对齐，尚未执行 Item 的业务构造。
2. `construct_at` 完成初始化。若构造抛异常，完整 Item 没构造成功，不能再调用它的析构；但原始存储必须归还。
3. 正常路径先 `destroy_at` 结束对象生命周期，再 `deallocate` 归还存储，责任不能颠倒。

**追问：普通 `new Item(true)` 构造失败会泄漏吗？** 正常 new 表达式会调用匹配的释放函数回收刚分配的存储。这里自己拆开了分配和构造，才需要自己补上异常清理。

**下一问：** 手动写这么多清理路径容易错。怎样把资源责任封装进一个可以复制、移动的类？

---

<a id="q04"></a>
## 04｜P0：手写资源类，怎样同时保证深拷贝、移动和异常安全？

**面试问题：** 实现一个整数缓冲区，解释五法则、copy-and-swap 和移动后状态。

**核心回答：** 拷贝创建独立资源；移动转交资源；析构只清理自己拥有的资源。赋值先准备新状态，成功后再提交，可以避免“旧数据删了、新数据却分配失败”。业务类若能用标准容器表达资源，应优先零法则。

**可运行代码：**

```cpp
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

class Buffer {
    std::size_t size_ = 0;
    int* data_ = nullptr;
public:
    explicit Buffer(std::size_t n = 0)
        : size_(n), data_(n ? new int[n]{} : nullptr) {}
    ~Buffer() { delete[] data_; }

    Buffer(const Buffer& other) : Buffer(other.size_) {
        if (size_) std::copy_n(other.data_, size_, data_);
    }
    Buffer(Buffer&& other) noexcept
        : size_(std::exchange(other.size_, 0)),
          data_(std::exchange(other.data_, nullptr)) {}

    Buffer& operator=(const Buffer& other) {
        Buffer prepared(other); // 可能失败，当前 *this 尚未改变。
        swap(prepared);         // 不抛提交，prepared 接手旧资源。
        return *this;
    }
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            Buffer prepared(std::move(other));
            swap(prepared);
        }
        return *this;
    }
    void swap(Buffer& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(data_, other.data_);
    }
    int& operator[](std::size_t i) { assert(i < size_); return data_[i]; }
    std::size_t size() const noexcept { return size_; }
};

int main() {
    Buffer a(2);
    a[0] = 7;
    Buffer b = a;
    b[0] = 9;
    assert(a[0] == 7);
    Buffer c = std::move(a);
    assert(a.size() == 0 && c[0] == 7);
    b = c;
    b = b;
    assert(b[0] == 7);
    c = std::move(b);
    c = std::move(c);
    assert(b.size() == 0 && c[0] == 7);
}
```

**执行过程：** `Buffer b = a` 分配新数组，故修改 b 不影响 a。`Buffer c = std::move(a)` 只转移地址，数组元素没有被逐个移动。拷贝赋值临时对象接手旧资源后离开作用域，由它析构旧数组。

**保证与代价：** 对这个 int 缓冲区，复制元素不会抛，可能失败的关键步骤是分配；copy-and-swap 提供强保证，代价是可能重新分配且不复用旧容量。若改为任意 T，还要处理元素复制抛异常和已构造元素计数。

**特别注意：** 本类主动承诺移动后为空；不能据此推断所有自定义类型或所有标准库类型都如此。用户自定义类的移动后状态取决于其契约。

**下一问：** 代码里出现 `std::move` 就一定调用移动构造吗？

---

<a id="q05"></a>
## 05｜P0：`std::move`、`std::forward` 与引用折叠怎样决定重载？

**核心回答：** `move` 产生可供移动重载选择的表达式，不执行资源转移。命名的右值引用变量作为表达式仍是左值；转发引用需要 `forward` 恢复调用方的值类别。const 也不会被 move 自动去掉。

**可运行代码：**

```cpp
#include <cassert>
#include <utility>

int identify(int&) { return 1; }
int identify(const int&) { return 2; }
int identify(int&&) { return 3; }

template<class T>
int relay(T&& value) {
    return identify(std::forward<T>(value));
}

struct Probe {
    static inline int copies = 0, moves = 0;
    Probe() = default;
    Probe(const Probe&) { ++copies; }
    Probe(Probe&&) noexcept { ++moves; }
};

int main() {
    int x = 0;
    const int cx = 0;
    int&& named = 1;
    assert(identify(named) == 1);
    assert(relay(x) == 1);
    assert(relay(cx) == 2);
    assert(relay(1) == 3);
    assert(relay(std::move(x)) == 3);
    const Probe original;
    Probe result(std::move(original));
    (void)result;
    assert(Probe::copies == 1 && Probe::moves == 0);
}
```

**推导过程：**

| 调用 | T | 折叠后的形参类型 | 转发给 identify |
| --- | --- | --- | --- |
| `relay(x)` | `int&` | `int&` | 左值 |
| `relay(cx)` | `const int&` | `const int&` | const 左值 |
| `relay(1)` | `int` | `int&&` | 右值 |

引用折叠中，只要出现 `&`，结果就是 `&`；仅 `&&` 与 `&&` 组合仍为 `&&`。最后的 `move(original)` 是 const 右值，不能绑定需要非 const 的 `Probe&&`，于是选择 `const Probe&` 拷贝构造。

**追问：`const T&&` 是转发引用吗？** 不是。`T&&` 还要求 T 是这里可推导的模板参数；已确定类型的 `Widget&&`、某些类模板成员中的 `T&&` 都不能机械称为转发引用。

---

<a id="q06"></a>
## 06｜P0：返回值优化为什么比移动更进一步？

**面试问题：** 返回一个禁止拷贝、禁止移动的对象，为何仍可编译？`return std::move(local)` 是否总能优化？

**可运行代码：**

```cpp
#include <cassert>

struct Immovable {
    static inline int alive = 0;
    Immovable() { ++alive; }
    Immovable(const Immovable&) = delete;
    Immovable(Immovable&&) = delete;
    ~Immovable() { --alive; }
};

Immovable make_value() { return Immovable{}; }

int main() {
    {
        Immovable value = make_value();
        (void)value;
        assert(Immovable::alive == 1);
    }
    assert(Immovable::alive == 0);
}
```

**剖析：** C++17 起，这个同类型纯右值返回路径直接初始化结果对象，不需要先造临时再搬一次，因此删除拷贝和移动也合法。析构函数仍须可访问且未删除。

**追问：`T local; return local;` 呢？** 这是具名返回值优化 NRVO 的常见场景，优化不是同样的强制保证，需要考虑未实施时可用的返回路径。`return std::move(local)` 通常会破坏 NRVO 的适用条件，并不值得习惯性添加。

**面试表达：** 先说明当前 return 是纯右值还是具名局部变量，再讨论消除、隐式移动和拷贝；不要凭某一次日志就断言所有编译器都调用相同次数。

规则校核：[C++ 草案：拷贝消除](https://eel.is/c++draft/class.copy.elision)。

---

<a id="q07"></a>
## 07｜P0：现场手写一个最小 `unique_ptr`

**面试问题：** 支持析构、移动、reset、release；为什么不能默认拷贝？

**核心回答：** 一个对象只有一个删除责任人。拷贝裸地址会制造两个责任人；移动则让源对象交出责任。这个教学版本只管理单个、可经 T* 正确 delete 的对象，约定析构不抛；不支持数组和自定义删除器。

**可运行代码：**

```cpp
#include <cassert>
#include <type_traits>
#include <utility>

template<class T>
class Unique {
    T* ptr_ = nullptr;
public:
    explicit Unique(T* p = nullptr) noexcept : ptr_(p) {}
    Unique(const Unique&) = delete;
    Unique& operator=(const Unique&) = delete;
    Unique(Unique&& rhs) noexcept : ptr_(rhs.release()) {}
    Unique& operator=(Unique&& rhs) noexcept {
        if (this != &rhs) reset(rhs.release());
        return *this;
    }
    ~Unique() { delete ptr_; }
    T* get() const noexcept { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T* release() noexcept { return std::exchange(ptr_, nullptr); }
    void reset(T* next = nullptr) noexcept {
        delete std::exchange(ptr_, next);
    }
};

struct Item {
    static inline int alive = 0;
    Item() { ++alive; }
    ~Item() { --alive; }
};

int main() {
    static_assert(!std::is_copy_constructible_v<Unique<Item>>);
    static_assert(std::is_nothrow_move_constructible_v<Unique<Item>>);
    Unique<Item> a(new Item);
    Unique<Item> b(std::move(a));
    assert(!a && b && Item::alive == 1);
    Item* raw = b.release();
    assert(!b && Item::alive == 1);
    a.reset(raw);
    a.reset();
    assert(Item::alive == 0);
}
```

**关键路径：** 移动构造经 `release()` 取得旧地址同时清空来源；移动赋值先从来源脱离资源，再 reset 接管；reset 替换内部状态后删除旧对象。`release()` 不析构对象，所以忽略其返回值会泄漏。

**易错代码：** `p.reset(p.get())` 会删除正在继续保存的那个地址，之后悬垂；这不是自移动检查能解决的问题。把同一个裸地址交给两个 Unique 同样违背独占前提。

**追问：为什么 `vector<unique_ptr<T>>` 扩容后 T 的地址通常没变？** 容器移动的是指针包装器，堆上的 T 没有被搬迁；保存包装器地址与保存 T 地址是两回事。

---

<a id="q08"></a>
## 08｜P0：`shared_ptr`、`weak_ptr` 的两阶段销毁与原子提升

**面试问题：** 对象已析构，weak 为什么还能使用？检查 expired 后是否就能访问对象？

**核心回答：** 对象和共享状态的生命周期不同。最后一个强拥有者离开时销毁对象；弱观察者还需要保留控制块，以便判断能否取得所有权。使用 `lock()` 原子尝试取得强引用，不能把 expired 与实际访问拆成两个步骤。

**可运行代码：**

```cpp
#include <cassert>
#include <memory>

struct Item {
    static inline int alive = 0;
    int value = 42;
    Item() { ++alive; }
    ~Item() { --alive; }
};

int main() {
    std::weak_ptr<Item> weak;
    {
        auto first = std::make_shared<Item>();
        weak = first;
        auto second = weak.lock();
        assert(first.use_count() == 2);
        first.reset();
        assert(second->value == 42 && Item::alive == 1);
        second.reset();
        assert(Item::alive == 0 && weak.expired());
    }
    assert(!weak.lock());
    weak.reset();
}
```

**手推状态（单线程）**：首次 make_shared 有 1 个强拥有者；lock 成功后为 2；reset(first) 后还有 second 保活；reset(second) 析构 Item；最后 weak.reset 才释放最后的弱观察关系。控制块的具体 weak 计数编码是实现细节，不能把某种“弱计数额外加一”的实现约定当作标准接口。

**追问一：手写 lock 为什么需要 CAS？** 先读取 strong>0，再无条件自增，中间可能被另一线程降为 0。CAS 必须把“仍为非零的预期值才增加”作为一个原子更新；失败刷新预期值，读到 0 就停止，不能复活对象。返回 SharedPtr 时不能重复增加已由 CAS 取得的那次强引用。

**追问二：make_shared 的取舍？** 主流实现把对象与控制块合并分配，减少分配次数；对象析构后，仍有 weak 时那块合并存储通常尚不能归还。对象已析构与内存尚保留是两回事。标准 `shared_ptr(new T)` 若控制块构造失败会清理传入对象，也有异常安全保证，不能把某个教学实现的泄漏写成标准库行为。

**追问三：三层线程安全？** 不同 shared_ptr 副本可并发维护同一所有权组；同一包装器实例被并发修改需要锁或 `atomic<shared_ptr<T>>`；被管理对象自身仍需要独立同步。`use_count()==1` 只是快照，不能替代锁。

规则校核：[shared_ptr 构造与观察接口](https://eel.is/c++draft/util.smartptr.shared)、[weak_ptr::lock 原子语义](https://eel.is/c++draft/util.smartptr.weak.obs)。更完整的手写控制块逐段解读见 [手搓智能指针代码解读](./手搓智能指针代码解读.md)。

---

<a id="q09"></a>
## 09｜P0：回调捕获 `this`，为什么对象销毁后会崩？

**面试问题：** 异步队列中的回调晚于页面销毁才执行，应该捕获什么？

**核心回答：** 捕获 this 只复制指针，不保活对象。需要保证执行就捕获强引用，需要对象仍存在才执行就捕获弱引用。选择会决定页面关闭后任务是否继续保活页面。

**可运行代码：**

```cpp
#include <cassert>
#include <functional>
#include <memory>
#include <vector>

struct Page : std::enable_shared_from_this<Page> {
    void schedule(std::vector<std::function<void()>>& queue, int& calls) {
        queue.push_back([weak = weak_from_this(), &calls] {
            if (auto self = weak.lock()) {
                (void)self;
                ++calls;
            }
        });
    }
};

int main() {
    int calls = 0; // 活到所有回调销毁以后。
    std::vector<std::function<void()>> queue;
    auto page = std::make_shared<Page>();
    page->schedule(queue, calls);
    queue[0]();
    assert(calls == 1);
    page.reset();
    queue[0]();
    assert(calls == 1);
}
```

**剖析：** 第一次执行时 lock 成功，局部 self 保活对象直到回调结束。reset(page) 后只有弱引用，第二次执行不能再取得对象，安全跳过。`calls` 按引用捕获是因为示例明确保证它活得更久；换成真实异步线程，还要同步对 calls 的并发读写。

**追问一：为何不能 `shared_ptr<Page>(this)`？** 会建立第二个所有权组；两个控制块各自释放同一对象。`shared_from_this()` 复用既有控制块，但须已由兼容的 shared_ptr 接管；在构造函数里调用通常太早，可能抛 `bad_weak_ptr`。

**追问二：捕获 shared_ptr 为什么会循环引用？** 若对象自己存放回调，回调又强持有对象，就是 `对象 → 回调 → 对象`。弱捕获或显式断开订阅才能解除这条所有权环。

---

<a id="q10"></a>
## 10｜P0：虚函数、默认实参、构造期分派和对象切片

**面试问题：** 同一个派生对象，经基类指针和派生对象直接调用，为什么参数可能不同？

**可运行代码：**

```cpp
#include <cassert>
#include <memory>

struct Base {
    int during_construction;
    Base() : during_construction(kind()) {}
    virtual int kind() const { return 1; }
    virtual int score(int n = 1) const { return 10 + n; }
    virtual ~Base() = default;
};
struct Derived : Base {
    static inline int destroyed = 0;
    int kind() const override { return 2; }
    int score(int n = 2) const override { return 20 + n; }
    ~Derived() override { ++destroyed; }
};

int main() {
    {
        auto owner = std::make_unique<Derived>();
        Base* base = owner.get();
        assert(owner->during_construction == 1);
        assert(base->kind() == 2);
        assert(base->score() == 21);
        assert(owner->score() == 22);
        Base sliced = *owner;
        assert(sliced.kind() == 1 && sliced.score() == 11);
        std::unique_ptr<Base> polymorphic = std::move(owner);
    }
    assert(Derived::destroyed == 1);
}
```

**执行过程：**

1. 构造 Base 子对象时，kind 调用 Base 实现，记录 1；完整对象构造后，经 Base* 调用分派到 Derived，返回 2。
2. 默认实参由调用位置的静态类型决定；函数体由虚分派决定，因此分别得到 `20+1` 与 `20+2`。
3. `Base sliced = *owner` 生成独立 Base，派生部分被切掉，不再具有 Derived 的动态类型。
4. `unique_ptr<Base>` 最终经 Base* 删除对象，虚析构确保 Derived 析构运行。

**追问：怎样避免这些接口陷阱？** 多态对象经引用或指针传递；重写函数使用 override；避免在虚函数层级中给不同的默认实参。构造函数可以调用虚函数，但不能期待它分派到尚未构造的派生部分；也不能调用当前层尚无有效实现的纯虚接口。

**主流实现与标准区别：** vptr/vtable 是常见 ABI 实现，不保证虚表就在对象第一个字段，也不保证只有一张表。规则校核：[虚函数与重写](https://eel.is/c++draft/class.virtual)。

---

<a id="q11"></a>
## 11｜P1：四种 cast 如何选？重新解释地址能绕过类型规则吗？

**核心回答：** cast 应表达明确的转换意图。向下转型需要运行时检查时用 dynamic_cast；已证明关系的转换用 static_cast；const_cast 只改访问限定，不能让真正的 const 对象合法可写；reinterpret_cast 不能赋予任意地址正确的类型、对齐和生命周期。

**可运行代码：**

```cpp
#include <bit>
#include <cassert>
#include <cstdint>
#include <typeinfo>

struct Base { virtual ~Base() = default; };
struct Derived : Base { int value = 7; };
struct Other : Base {};

int main() {
    Derived object;
    Base* base = &object;
    auto* derived = dynamic_cast<Derived*>(base);
    assert(derived && derived->value == 7);
    assert(dynamic_cast<Other*>(base) == nullptr);
    bool threw = false;
    try { (void)dynamic_cast<Other&>(*base); }
    catch (const std::bad_cast&) { threw = true; }
    assert(threw);

    int writable = 1;
    const int* read_only_path = &writable;
    *const_cast<int*>(read_only_path) = 2;
    assert(writable == 2);

    static_assert(sizeof(float) == sizeof(std::uint32_t));
    float value = 1.0f;
    auto bits = std::bit_cast<std::uint32_t>(value);
    assert(std::bit_cast<float>(bits) == value);
}
```

**剖析：** dynamic_cast 指针失败返回空，引用失败抛异常。这里 const_cast 合法，是因为原对象本来就是非 const；若原本声明为 `const int`，去 const 后写入就是未定义行为。bit_cast 创建新值以复制对象表示，没有通过错误类型指针去解引用原对象。

**边界：** bit_cast 要求相同大小、两端可平凡复制，目标表示还必须有可解释的有效值。示例检查本平台 float 与 uint32_t 等大，不假设所有平台的 float 格式。网络序列化还要考虑字节序、协议编码和 padding，不能直接把整个结构体内存当作通用协议。

---

<a id="q12"></a>
## 12｜P1：模板约束和 `if constexpr` 分别解决什么？

**面试问题：** 写一个返回序列长度的泛型函数：容器用 size，内置数组取 N；不支持的类型应尽早被拒绝。

**可运行代码：**

```cpp
#include <cassert>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <vector>

template<class T>
concept HasSize = requires(const T& value) {
    { value.size() } -> std::convertible_to<std::size_t>;
};

template<HasSize T>
std::size_t length(const T& value) { return value.size(); }

template<class T, std::size_t N>
constexpr std::size_t length(const T (&)[N]) { return N; }

template<class T>
constexpr int category() {
    if constexpr (std::is_integral_v<T>) return 1;
    else return 2;
}

int main() {
    int array[3]{};
    std::vector<int> values(4);
    assert(length(array) == 3 && length(values) == 4);
    static_assert(!HasSize<int>);
    static_assert(category<int>() == 1);
    static_assert(category<double>() == 2);
}
```

**逐段剖析：** `requires` 检查表达式和返回类型能力，决定模板能否参与调用；它不实际执行 size。数组形参使用引用，避免退化成 `T*` 而丢失长度。`if constexpr` 则在某次模板实例化中选择实现分支，用于同一接口内部的编译期差异。

**追问一：它与 SFINAE 是什么关系？** SFINAE 利用替换失败移除候选，concepts 将约束直接写在接口上，通常更易读、诊断更明确；不意味着所有模板错误都会被静默忽略。

**追问二：`constexpr` 一定编译期执行吗？** 不一定，也可运行期调用。需要强制编译期求值的函数使用 C++20 consteval；constexpr 变量的初始化则要求常量表达式。

---

<a id="q13"></a>
## 13｜P1：`function`、`optional`、`variant` 分别擦除了什么、保留了什么？

**核心回答：** function 用固定调用签名包装不同 callable；optional 表示某种固定 T 是否存在；variant 在编译期列出的类型集合中保存一个活跃项。不要把空值、业务错误、任意类型混成一个 void*。

**可运行代码：**

```cpp
#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

using Result = std::variant<int, std::string>;

std::optional<int> lookup(bool found) {
    if (found) return 7;
    return std::nullopt;
}

int main() {
    std::function<int(int)> transform = [bias = 3](int x) { return x + bias; };
    assert(transform(4) == 7);
    auto found = lookup(true);
    assert(found && *found == 7);
    assert(lookup(false).value_or(-1) == -1);

    auto describe = [](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int>) return std::to_string(value);
        else return value;
    };
    Result result = 42;
    assert(std::visit(describe, result) == "42");
    result = std::string("not found");
    assert(std::visit(describe, result) == "not found");
}
```

**剖析：** lambda 闭包有自己的具体类型，赋给 function 后调用方只看到 `int(int)`。visit 则必须为所有备选项形成合法调用；这里统一返回 string，避免各分支返回类型不一致。

**追问一：`std::function` 是否总会堆分配？** 不一定，小对象优化由实现和目标类型共同决定；也不能承诺任意“小 lambda”都零分配。C++20 function 要求目标可拷贝，捕获 unique_ptr 的仅可移动闭包不能直接装进去；C++23 的 move_only_function 提供另一种选择。

**追问二：variant 会没有活跃值吗？** 换型构造抛异常时可能进入 valueless_by_exception 状态；不能把它当作永远可直接 get 的 union。optional.value 在无值时抛异常，解引用 optional 则要求它确实有值。

---

<a id="q14"></a>
## 14｜P0：`vector` 扩容为什么有时拷贝而不是移动？

**面试问题：** 如何在迁移元素中途抛异常时维持旧容器？`noexcept` 的作用是什么？

**可运行代码：**

```cpp
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

struct Element {
    static inline int remaining = -1;
    int value;
    explicit Element(int x) : value(x) {}
    Element(const Element& rhs) : value(rhs.value) {
        if (remaining == 0) throw std::runtime_error("copy failure");
        if (remaining > 0) --remaining;
    }
    Element(Element&& rhs) noexcept(false) : value(rhs.value) {
        rhs.value = -1;
    }
};

int main() {
    static_assert(std::is_same_v<
        decltype(std::move_if_noexcept(std::declval<Element&>())),
        const Element&>);
    std::vector<Element> data;
    data.reserve(2);
    data.emplace_back(10);
    data.emplace_back(20);
    auto old_capacity = data.capacity();
    Element::remaining = 1;
    bool threw = false;
    try { data.reserve(old_capacity + 1); }
    catch (const std::runtime_error&) { threw = true; }
    assert(threw);
    assert(data.size() == 2 && data.capacity() == old_capacity);
    assert(data[0].value == 10 && data[1].value == 20);
    Element::remaining = -1;
    data.reserve(old_capacity + 1);
    assert(data[0].value == 10 && data[1].value == 20);
}
```

**剖析：** 这段程序在本讲义验证的标准库上走拷贝迁移：第一次拷贝成功，第二次抛异常；新区域中已经构造的元素必须销毁，新区域释放，旧区域仍保留。`move_if_noexcept` 的返回类型已用静态断言确认，但标准库不必在源码中真的调用这个同名函数。

**适用边界：** 元素可复制时，用复制有机会保留强保证；元素仅可移动且移动又会抛时，某些操作不能维持同样强的回滚保证。不能一概声称所有 vector 操作都“失败完全不变”。

**追问：reserve 与 resize？** reserve 准备存储容量，不增加活跃元素；resize 改变 size，可能构造或销毁元素。逐次 `reserve(size()+1)` 可能破坏几何增长的优势，使总体迁移接近二次复杂度。

规则校核：[vector 修改操作的异常与失效规则](https://eel.is/c++draft/vector.modifiers)。

---

<a id="q15"></a>
## 15｜P0：循环中删除元素，怎样避免迭代器失效？

**面试问题：** 为什么 erase 后继续 `++it` 可能漏删或崩溃？如何一次删除所有偶数？

**可运行代码：**

```cpp
#include <algorithm>
#include <cassert>
#include <vector>

int main() {
    std::vector<int> data{1, 2, 2, 3, 4};
    for (auto it = data.begin(); it != data.end();) {
        if (*it % 2 == 0) it = data.erase(it);
        else ++it;
    }
    assert((data == std::vector<int>{1, 3}));

    data = {1, 2, 2, 3, 4};
    auto removed = std::erase_if(data, [](int x) { return x % 2 == 0; });
    assert(removed == 3 && (data == std::vector<int>{1, 3}));

    std::vector<int> stable;
    stable.reserve(8);
    stable.push_back(10);
    int* first = &stable[0];
    stable.push_back(20); // 未重新分配。
    assert(*first == 10);
}
```

**剖析：** erase 返回删除后可继续遍历的位置，原 it 不再可用；刚搬过来的那个元素必须重新检查，因此成功删除分支不能再 ++。连续删除多个相邻偶数是很好的边界测试。

**复杂度追问：** vector 每次中间 erase 都可能移动尾部元素，上面的手写循环最坏 O(n²)。C++20 erase_if 或 erase-remove 惯用法先压缩保留元素，再一次删尾部，可做到线性处理；list 的删除成本和局部性又不同。

**失效要分开说：** vector 扩容使全部元素地址失效；未扩容尾插保留已有元素引用，但旧 end 失效；中间 erase 使删除位置及之后的迭代器和引用失效。reserve 也不能保证后续中间删除的引用稳定。

---

<a id="q16"></a>
## 16｜P0：`unordered_map` 重新分桶后，引用和迭代器都失效吗？

**核心回答：** 不能套用 vector 的规则。标准 unordered 容器的 rehash 使迭代器失效，但保留元素指针/引用；删除元素才使指向被删元素的引用失效。map 适合有序遍历和范围查找，unordered_map 平均查找快但存在哈希最坏退化。

**可运行代码：**

```cpp
#include <cassert>
#include <map>
#include <string>
#include <unordered_map>

int main() {
    std::unordered_map<int, std::string> table{{1, "one"}};
    auto before = table.find(1);
    std::string* value = &before->second;
    table.rehash(table.bucket_count() * 2 + 1);
    assert(*value == "one"); // 引用/指针仍有效。
    // 不再使用 before；重新 find 取得新迭代器。
    assert(table.find(1)->second == "one");
    const auto count = table.size();
    assert(table.find(9) == table.end() && table.size() == count);
    (void)table[9];
    assert(table.size() == count + 1);

    std::map<int, int> ordered{{10, 1}, {30, 3}, {20, 2}};
    assert(ordered.lower_bound(15)->first == 20);
}
```

**剖析：** 保存的 value 指向元素本身，rehash 调整桶结构不使它失效；before 属于迭代机制，不能继续解引用。`operator[]` 查不存在的键会插入默认值，因此只读查询应选 find/contains/at，并明确缺失处理。

**追问一：哈希与相等关系必须满足什么？** 判定相等的 key 必须有相同 hash；hash 相同不代表相等。不要用不同的大小写规则分别实现 hash 和 equality。

**追问二：map 一定是红黑树吗？** 主流实现通常如此，但标准主要约束接口和复杂度。不能只背平均 O(1) 就断言哈希表必然比树快，还要考虑数据规模、缓存、分配、范围查询及恶意碰撞。

规则校核：[无序关联容器失效规则](https://eel.is/c++draft/unord.req)。

---

<a id="q17"></a>
## 17｜P0：手写 lower_bound，怎样证明边界正确？

**面试问题：** 求第一个不小于 target 的位置，空数组和重复元素如何处理？排序比较器为什么不能写 `<=`？

**可运行代码：**

```cpp
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

std::size_t first_not_less(const std::vector<int>& data, int target) {
    std::size_t left = 0, right = data.size();
    while (left < right) {
        const auto mid = left + (right - left) / 2;
        if (data[mid] < target) left = mid + 1;
        else right = mid;
    }
    return left;
}

int main() {
    assert(first_not_less({}, 2) == 0);
    std::vector<int> data{1, 2, 2, 4};
    assert(first_not_less(data, 2) == 1);
    assert(first_not_less(data, 3) == 3);
    assert(first_not_less(data, 5) == data.size());
    std::vector<int> unsorted{3, 1, 2, 2};
    std::sort(unsorted.begin(), unsorted.end(), [](int a, int b) {
        return a < b;
    });
    assert(std::is_sorted(unsorted.begin(), unsorted.end()));
}
```

**不变量：** `[0,left)` 都小于 target，`[right,n)` 都不小于 target，未判定区间为 `[left,right)`。当 mid 小于 target，mid 也可排除；否则 mid 可能就是答案，收缩 right 到 mid 而非 mid-1。结束时 left==right，答案允许为 n。

**复杂度：** 数组随机访问下 O(log n) 时间、O(1) 额外空间。标准 lower_bound 可接收非随机访问迭代器，但移动迭代器的总成本可能线性；对 map 应优先调用成员 lower_bound。

**比较器追问：** 排序要求严格弱序，最基本有 `comp(x,x)==false`。使用 `<=` 连自身都判“严格在前”，违背要求；这不是仅仅排序结果不稳定的问题。

---

<a id="q18"></a>
## 18｜P0：完整手写 LRU，并解释异常时的双结构一致性

**面试问题：** get/put 平均 O(1)，更新视为访问，容量 0 要可用。

**核心回答：** 链表保存访问顺序，头部最近使用；哈希表定位链表节点。两个结构必须同时包含同一组键。以下固定 int key/value，避免泛型异常条件掩盖核心逻辑；不提供线程安全。

**可运行代码：**

```cpp
#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

class Lru {
    using List = std::list<std::pair<int, int>>;
    std::size_t capacity_;
    List order_;
    std::unordered_map<int, List::iterator> index_;
public:
    explicit Lru(std::size_t capacity) : capacity_(capacity) {}
    Lru(const Lru&) = delete;
    Lru& operator=(const Lru&) = delete;
    Lru(Lru&&) = delete;
    Lru& operator=(Lru&&) = delete;

    std::optional<int> get(int key) {
        auto found = index_.find(key);
        if (found == index_.end()) return std::nullopt;
        order_.splice(order_.begin(), order_, found->second);
        return found->second->second;
    }
    void put(int key, int value) {
        if (capacity_ == 0) return;
        if (auto found = index_.find(key); found != index_.end()) {
            found->second->second = value;
            order_.splice(order_.begin(), order_, found->second);
            return;
        }
        order_.emplace_front(key, value);
        try {
            index_.emplace(key, order_.begin());
        } catch (...) {
            order_.pop_front(); // 哈希表登记失败，撤销新链表节点。
            throw;
        }
        if (order_.size() > capacity_) {
            auto last = std::prev(order_.end());
            index_.erase(last->first);
            order_.pop_back();
        }
    }
};

int main() {
    Lru cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    assert(cache.get(1) == 10);
    cache.put(3, 30);
    assert(!cache.get(2) && cache.get(3) == 30);
    cache.put(1, 11);
    assert(cache.get(1) == 11);
    Lru empty(0);
    empty.put(1, 1);
    assert(!empty.get(1));
    Lru single(1);
    single.put(1, 10);
    single.put(2, 20);
    assert(!single.get(1) && single.get(2) == 20);
}
```

**手推执行：** put(1)、put(2) 后顺序 `[2,1]`；get(1) 经 splice 改成 `[1,2]`，节点地址不变；put(3) 暂时得到 `[3,1,2]`，淘汰尾部 2，两个结构同时移除它。

**异常追问：** 新链表节点构造失败时结构未变；链表成功但哈希表分配失败时回滚新节点。先登记成功再淘汰旧资源，可以避免“新项加失败，却把旧缓存丢了”。这个 int 版本的尾部删除路径不会因用户自定义 hash/value 操作抛异常；推广到模板时需重新审视这些条件。

**为什么删掉拷贝？** 默认复制 list 会产生新节点，但默认复制 index 中的迭代器仍指向旧 list。必须禁止拷贝，或在自定义拷贝中重建索引。教学版连移动也显式禁用，避免把 allocator 与迭代器转移细节隐含在接口里。

**性能与工程边界：** get/put 平均 O(1)，哈希最坏可能 O(n)，空间 O(capacity)。get 也修改链表顺序，因此多线程共享时不能把它当作纯读操作只加读锁。

---

<a id="q19"></a>
## 19｜P0：两个对象之间转账，怎样保护不变量并避免死锁？

**面试问题：** 一个线程 A→B，另一个线程 B→A。各自先锁来源再锁目标会怎样？

**核心回答：** 锁保护的是一次完整状态转换。若每个线程先拿一把锁再等对方，可能形成循环等待。用统一全局锁序，或用 scoped_lock 同时取得多把互斥量；相同对象必须先特殊处理，不能重复锁同一非递归 mutex。

**可运行代码：**

```cpp
#include <cassert>
#include <mutex>
#include <thread>

struct Account {
    int balance = 1000;
    std::mutex mutex;
};

bool transfer(Account& from, Account& to, int amount) {
    if (amount < 0) return false;
    if (&from == &to) return true;
    std::scoped_lock lock(from.mutex, to.mutex);
    if (from.balance < amount) return false;
    from.balance -= amount;
    to.balance += amount;
    return true;
}

int main() {
    Account a, b;
    {
        std::jthread first([&] {
            for (int i = 0; i < 1000; ++i) (void)transfer(a, b, 1);
        });
        std::jthread second([&] {
            for (int i = 0; i < 1000; ++i) (void)transfer(b, a, 1);
        });
    }
    assert(a.balance == 1000 && b.balance == 1000);
    assert(transfer(a, a, 10));
}
```

**剖析：** from 的扣款和 to 的入款由同一个临界区保护，不会被另一笔转账插在两次更新之间。jthread 离开内层作用域时 join，主线程随后读取余额时两个 worker 已结束。示例金额范围很小；真实接口还需防止余额溢出并定义精度。

**追问一：把余额改成 atomic 就够了吗？** 单字段原子不自动保证两个账户之间的复合约束。余额足够检查、扣除、增加是否允许中间状态被观察，都需要协议设计。

**追问二：scoped_lock 能解决所有死锁吗？** 不能。持锁调用回调、等待 future、递归获取同一锁或跨系统资源依赖仍可能死锁。它解决的是这一组锁的获取问题。

---

<a id="q20"></a>
## 20｜P0：手写可关闭的有界阻塞队列

**面试问题：** 队列空时等待、满时阻塞；关闭后生产者被拒绝，消费者排空已有数据后退出。如何保证所有等待者都能醒来？

**核心回答：** 状态保存在受 mutex 保护的队列与 closed 标志里；通知本身不保存“许可证”。等待谓词必须同时考虑数据条件和关闭条件。有界容量提供背压，避免生产速度持续高于消费速度时无限涨内存。

**可运行代码：**

```cpp
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>

class BoundedQueue {
    std::size_t capacity_;
    std::queue<int> data_;
    bool closed_ = false;
    std::mutex mutex_;
    std::condition_variable not_empty_, not_full_;
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("zero capacity");
    }
    bool push(int value) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] {
            return closed_ || data_.size() < capacity_;
        });
        if (closed_) return false;
        data_.push(value);
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }
    std::optional<int> pop() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, [&] { return closed_ || !data_.empty(); });
        if (data_.empty()) return std::nullopt;
        int result = data_.front();
        data_.pop();
        lock.unlock();
        not_full_.notify_one();
        return result;
    }
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
};

int main() {
    BoundedQueue queue(2);
    int sum = 0;
    {
        std::jthread consumer([&] {
            while (auto value = queue.pop()) sum += *value;
        });
        std::jthread producer([&] {
            for (int i = 1; i <= 100; ++i) {
                const bool accepted = queue.push(i);
                assert(accepted);
            }
            queue.close();
        });
    }
    assert(sum == 5050);
    assert(!queue.push(101) && !queue.pop());
    queue.close(); // 允许重复关闭。

    BoundedQueue drain(2);
    drain.push(7);
    drain.close();
    assert(drain.pop() == 7 && !drain.pop());
    bool rejected = false;
    try { BoundedQueue invalid(0); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
```

**执行过程：** producer 填满后等待 not_full；consumer 取一个后通知它。close 将 closed 改为 true，再唤醒两类等待者。消费者即使已关闭也先处理剩余数据；只有关闭且为空时才返回 nullopt。生产者一旦观察关闭，无论有没有空间都拒绝新任务。

**为什么 wait 不能配一个裸 if？** 可能虚假唤醒，也可能其他消费者先取走数据；wait(lock,predicate) 会在重新持锁后反复检查。所有访问 closed 和队列状态的路径使用同一把锁。

**边界必须说清：** 队列对象必须比所有使用者活得更久，先 close、再 join、最后析构。close 不等于取消正在处理的工作。这个版本专注 int 队列，不实现分配失败后的全局关闭策略；生产者线程还应在真实系统中捕获任务/分配异常，确保失败路径也能通知消费者退出。

**线程池追问：** worker 是反复 pop 并执行的循环，但还需决定任务返回值、异常、拒绝策略、drain/cancel，以及 worker 内嵌套提交后等待造成的线程池耗尽。

规则校核：[条件变量等待与通知](https://eel.is/c++draft/thread.condition.condvar)。

---

<a id="q21"></a>
## 21｜P0：任务结果与异常怎样跨线程返回？jthread 会强杀线程吗？

**核心回答：** packaged_task 把可调用对象的结果或异常放进共享状态，future 负责取得结果；异常在 get 时重抛。jthread 析构请求停止并 join，但停止是协作协议，任务须主动检查或使用可停止的等待。

**可运行代码：**

```cpp
#include <atomic>
#include <cassert>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>

int main() {
    std::packaged_task<int(int)> task([](int x) { return x * x; });
    auto result = task.get_future();
    std::jthread worker(std::move(task), 7);
    assert(result.get() == 49);
    assert(!result.valid());

    std::packaged_task<int()> failing([]() -> int {
        throw std::runtime_error("task failure");
    });
    auto error = failing.get_future();
    std::jthread failed_worker(std::move(failing));
    bool caught = false;
    try { (void)error.get(); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught);

    std::promise<void> started;
    auto ready = started.get_future();
    std::atomic<bool> stopped{false};
    {
        std::jthread cancellable([&](std::stop_token stop) {
            started.set_value();
            while (!stop.stop_requested()) std::this_thread::yield();
            stopped.store(true, std::memory_order_relaxed);
        });
        ready.wait();
        cancellable.request_stop();
    }
    assert(stopped.load(std::memory_order_relaxed));
}
```

**剖析：** 第一个 task 被移动进 worker，future 留在调用线程；get 等待结果并消耗普通 future 的有效共享状态。第二个 task 的异常被包装层捕获，调用方从 get 得到它；裸线程函数异常若逃逸则可能 terminate。第三个任务主动检查 stop_token，收到停止请求后退出；主线程读取 stopped 前已 join。

**追问一：request_stop 能打断普通 cv.wait 吗？** 不能自动打断。可把关闭条件加入队列协议并通知，或使用支持 stop_token 的 condition_variable_any 等等待接口。只换成 jthread 并不保证任何阻塞任务都能退出。

**追问二：async 一定创建新线程吗？** 默认策略允许 deferred；明确需要异步执行时指定 `std::launch::async`。还要注意某些 async 返回 future 的析构可能等待任务完成，丢弃 future 不等于 fire-and-forget。

---

<a id="q22"></a>
## 22｜P0：一个 atomic 标志怎样安全发布普通数据？

**面试问题：** writer 写 data 后置 ready，reader 看到 ready 后读取 data。data 自己不是 atomic，为何有时合法、有时仍是数据竞争？

**可运行代码：**

```cpp
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

int main() {
    std::vector<int> payload;
    std::atomic<bool> ready{false};
    int observed = 0;
    {
        std::jthread reader([&] {
            while (!ready.load(std::memory_order_acquire))
                std::this_thread::yield();
            observed = payload[0] + payload[1];
        });
        std::jthread writer([&] {
            payload = {20, 22};
            ready.store(true, std::memory_order_release);
        });
    }
    assert(observed == 42);
}
```

**证明链：** `payload 的写` 在 writer 的 release store 之前；reader 的 acquire load 读到该 release 发布的 true；两者建立 synchronizes-with；通过传递性，payload 的写 happens-before reader 的读。主线程读取 observed 前，reader 也已 join。

**换成 relaxed 会怎样？** flag 本身仍原子，但不再为 payload 建立这条发布关系。不能因为某台 x86 机器运行正常就判定 C++ 程序正确。volatile 也不提供这样的线程同步。

**必须限定：** 这个示例只发布一次，发布后 writer 不再修改 payload。若重复加载新数据、清空 vector 或切换指针，还需双缓冲、锁、消息队列或带生命周期管理的快照协议。

**性能追问：** yield 忙等用于缩短示例，不是后台等待的默认方案。实际可用条件变量或 C++20 atomic::wait/notify，并保持同样的状态与内存序推理。

规则校核：[数据竞争与 happens-before](https://eel.is/c++draft/intro.races)、[原子内存序](https://eel.is/c++draft/atomics.order)。

---

<a id="q23"></a>
## 23｜P1：用 CAS 实现原子最大值，为什么要重试？

**面试问题：** 多线程更新“历史最大值”，`if (value > max) max = value` 为什么错误？

**可运行代码：**

```cpp
#include <atomic>
#include <cassert>
#include <limits>
#include <thread>

void update_max(std::atomic<int>& maximum, int value) {
    int seen = maximum.load(std::memory_order_relaxed);
    while (seen < value) {
        if (maximum.compare_exchange_weak(
                seen, value,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return;
        }
        // 失败时 seen 更新为当前观察值，或发生伪失败后再次尝试。
    }
}

int main() {
    std::atomic<int> maximum{std::numeric_limits<int>::min()};
    {
        std::jthread a([&] { update_max(maximum, 10); });
        std::jthread b([&] { update_max(maximum, 30); });
        std::jthread c([&] { update_max(maximum, 20); });
    }
    assert(maximum.load(std::memory_order_relaxed) == 30);
    update_max(maximum, -1);
    assert(maximum.load(std::memory_order_relaxed) == 30);
}
```

**错误执行序列：** A 读取旧最大值 0，准备写 10；B 写入 30；A 仍盲目写 10，就把较大值覆盖掉。CAS 要求 maximum 仍等于 seen 才提交 value，失败则重新判断最新状态。

**为什么 relaxed 足够？** 这里只维护一个独立统计整数，不发布任何关联对象；最终检查发生在工作线程 join 之后。若最大值还对应某个普通内存中的“最佳结果对象”，这份代码就不够，不能自动保证两个字段成对一致。

**追问：无锁栈也是这样一个 CAS 就完成了吗？** 不是。还涉及 ABA 和节点回收；tagged pointer 能帮助识别 ABA，但不能单独防止另一线程解引用已释放节点。hazard pointer、epoch 等需要完整的访问/回收协议。atomic<T> 也不一概保证底层 lock-free，需查询实现能力。

---

<a id="q24"></a>
## 24｜P0：Meyers 单例的线程安全到底覆盖哪一部分？

**面试问题：** 多线程第一次调用 instance 是否重复构造？初始化安全是否意味着成员操作都安全？

**可运行代码：**

```cpp
#include <atomic>
#include <cassert>
#include <thread>

class Service {
    std::atomic<int> requests_{0};
    Service() = default;
public:
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    static Service& instance() {
        static Service singleton;
        return singleton;
    }
    void record() { requests_.fetch_add(1, std::memory_order_relaxed); }
    int count() const { return requests_.load(std::memory_order_relaxed); }
};

int main() {
    Service* first = nullptr;
    Service* second = nullptr;
    {
        std::jthread a([&] {
            first = &Service::instance();
            first->record();
        });
        std::jthread b([&] {
            second = &Service::instance();
            second->record();
        });
    }
    assert(first == second && first->count() == 2);
}
```

**剖析：** C++11 起函数内 static 的初始化提供并发保护；第二个进入初始化的线程要等待完成。requests_ 另外用 atomic 保护，原因是单例初始化机制不保护对象后续的方法。

**追问一：构造抛异常怎么办？** 本次初始化未完成，后续调用可重试。递归进入仍在初始化中的同一个局部 static 则是需要避免的错误设计。

**追问二：放到头文件会怎样？** 示例类内成员函数隐式 inline。外部链接的 inline 函数可在多个翻译单元按规则定义，并共享对应局部 static；但若头文件里改成内部链接的 `static` 自由函数，各翻译单元会各有实体。inline 的 ODR 含义不等于必须内联优化。

**追问三：单例何时不适合？** 跨模块静态析构顺序、后台线程退出时机、测试隔离和依赖替换都会变复杂。需要不同配置或可替换实例时，可通过构造注入依赖，而不是全局到处 instance。

---

<a id="q25"></a>
## 25｜P1：手写“允许内存重叠”的拷贝，关键是什么？

**面试问题：** 将同一缓冲区的一段字节移动到另一段，为什么从前往后复制会覆盖尚未读到的数据？

**核心回答：** 目标位于源区间内部且在其后时，需要从后向前复制；其余情况下可向前复制。下面刻意将契约限定为同一个 byte 数组中的偏移，避免对任意不相关对象指针做不可靠的排序或相减。

**可运行代码：**

```cpp
#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <stdexcept>

void move_bytes(std::span<unsigned char> buffer,
                std::size_t dst, std::size_t src, std::size_t count) {
    const auto size = buffer.size();
    if (src > size || dst > size || count > size - src || count > size - dst)
        throw std::out_of_range("copy range");
    if (count == 0 || src == dst) return;
    if (dst > src && dst - src < count) {
        for (auto i = count; i > 0; --i)
            buffer[dst + i - 1] = buffer[src + i - 1];
    } else {
        for (std::size_t i = 0; i < count; ++i)
            buffer[dst + i] = buffer[src + i];
    }
}

int main() {
    std::array<unsigned char, 5> data{1, 2, 3, 4, 5};
    move_bytes(data, 1, 0, 4);
    assert((data == std::array<unsigned char, 5>{1, 1, 2, 3, 4}));
    move_bytes(data, 0, 1, 4);
    assert((data == std::array<unsigned char, 5>{1, 2, 3, 4, 4}));
    move_bytes(data, data.size(), data.size(), 0);
    bool rejected = false;
    try { move_bytes(data, 4, 0, 2); }
    catch (const std::out_of_range&) { rejected = true; }
    assert(rejected);
}
```

**逐段剖析：** 校验用 `count > size-src` 避免先算 src+count 溢出；依赖 `||` 从左到右短路，确认 src 不大于 size 后才做减法。逆序循环写 `i>0; --i`，避免无符号 i 从 0 下溢后继续循环。

**追问：memcpy 和 memmove？** memcpy 要求源/目标区域不重叠；memmove 按能处理重叠的语义拷贝。给 std::string、unique_ptr 这样的非平凡对象复制对象表示，不等于合法复制其所有权；对象复制必须服从该类型的构造/赋值语义。

---

<a id="q26"></a>
## 26｜P0：手写第 K 大，怎样处理重复元素和复杂度追问？

**面试问题：** 找第 K 大，K 从 1 开始；重复值占排名。实现随机化快速选择。

**可运行代码：**

```cpp
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

int kth_largest(std::vector<int>& data, std::size_t k) {
    if (k == 0 || k > data.size()) throw std::out_of_range("invalid k");
    const std::size_t target = data.size() - k;
    std::size_t left = 0, right = data.size();
    static thread_local std::mt19937 random(2026);
    for (;;) {
        std::uniform_int_distribution<std::size_t> pick(left, right - 1);
        const int pivot = data[pick(random)]; // 保存值，避免交换改变 pivot。
        std::size_t less = left, current = left, greater = right;
        while (current < greater) {
            if (data[current] < pivot) {
                std::swap(data[less++], data[current++]);
            } else if (data[current] > pivot) {
                std::swap(data[current], data[--greater]);
            } else {
                ++current;
            }
        }
        if (target < less) right = less;
        else if (target >= greater) left = greater;
        else return pivot;
    }
}

int main() {
    for (const auto& original : std::vector<std::vector<int>>{
             {3, 2, 1, 5, 6, 4}, {2, 2, 2}, {9}, {-1, 0, -1, 8}}) {
        auto sorted = original;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>{});
        for (std::size_t k = 1; k <= original.size(); ++k) {
            auto copy = original;
            assert(kth_largest(copy, k) == sorted[k - 1]);
        }
    }
    std::vector<int> empty;
    bool rejected = false;
    try { (void)kth_largest(empty, 1); }
    catch (const std::out_of_range&) { rejected = true; }
    assert(rejected);
}
```

**分区不变量：** `[left,less)` 小于 pivot，`[less,current)` 等于 pivot，`[current,greater)` 尚未处理，`[greater,right)` 大于 pivot。把较大值换到右边时，换回来的元素未知，所以 current 不能立即增加。

**为什么适合重复值？** 一次分区会集中所有等于 pivot 的元素；target 落在这段中即可返回，不必每次只排除一个相等元素。升序的目标索引为 n-k，先校验 k 避免无符号下溢。

**复杂度回答：** 随机枢轴模型下期望 O(n)，最坏 O(n²)；迭代分区使用 O(1) 随输入规模增长的额外空间，固定大小 PRNG 状态不随 n 增长。示例固定随机种子方便复现，不能抵抗了解序列的对抗性输入。堆方案为 O(n log k)、O(k) 空间；确定性最坏 O(n) 可讨论 BFPRT，但实现复杂、常数较大。

**工程追问：** 这里会重排输入；需要保留原数组时要复制或换算法。标准库中优先考虑 nth_element，并按其接口的复杂度保证说明，不要把自己实现的最坏界限直接套过去。

---

## 复习时怎样把这些题连问起来

| 起点 | 追问路径 | 应该能落到的代码 |
| --- | --- | --- |
| “你为什么返回引用？” | 推导类型 → 底层对象存活多久 → 是否应返回值 → RVO | 01、02、06 |
| “这个类拥有资源吗？” | 深拷贝 → 自赋值 → 移动 → 异常回滚 → 零法则 | 03、04、05、07 |
| “这个回调会不会崩？” | 捕获 this → 对象销毁 → weak.lock → 控制块 → 并发修改 | 08、09、19、22 |
| “STL 你用过吗？” | vector 扩容 → noexcept → 失效 → 哈希定位 → LRU | 14、15、16、18 |
| “怎样做异步加载？” | 队列背压 → 关闭唤醒 → future 异常 → 协作停止 → 数据发布 | 20、21、22 |
| “原子比锁快吗？” | 复合状态 → CAS 重试 → 竞争 → ABA → 回收和性能测量 | 19、23 |

## 常见错误答案的纠正

| 容易背错的话 | 应改成 |
| --- | --- |
| move 会把对象变空 | move 是转换；是否移动、移动后状态看类型与所调用操作 |
| 析构不会抛所以随便 noexcept | 设计清理函数不抛；异常逃出 noexcept 会 terminate |
| shared_ptr 的线程安全保护对象 | 它保护共享所有权的管理；对象字段要单独同步 |
| expired 为 false 就肯定活着 | 是瞬时观察；使用 lock 取得使用期间的强所有权 |
| make_shared 和 shared_ptr(new T) 只有前者异常安全 | 标准 shared_ptr 构造失败会清理指针；教学版遗漏清理是实现缺陷 |
| vector 预留容量后没有失效问题 | 中间插删仍会失效，旧 end 也不能随意保留 |
| unordered_map rehash 后元素指针失效 | 标准 unordered 容器保留元素指针/引用，迭代器失效 |
| 两个 atomic 就能维护两字段一致性 | 多步骤复合不变量需要锁或专门协议 |
| notify 相当于保存一张唤醒票 | 状态靠谓词保存，notify 只促使等待者重新检查 |
| 测试跑过能证明无数据竞争 | 测试只提供现象证据，正确性还需同步关系证明 |

## 如何编译和核对

每段程序可以独立保存为 `example.cpp`，用支持 C++20 的编译器运行：

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread example.cpp -o example.exe
.\example.exe
```

不要定义 NDEBUG，否则 assert 被关闭，验证效果会改变。运行成功通常没有打印，断言失败才会报错。并发代码验证不能替代内存模型证明；不同平台的 ABI、标准库分配策略和优化选择，也不能由一次运行结果推广成标准规定。

配套脚本 [check_interview_examples.ps1](../scripts/check_interview_examples.ps1) 会从本文提取每个完整代码块，逐段编译并执行，产物放在独立构建目录。本文规则引用链接指向持续更新的 C++ 草案，用于定位规则；示例只使用 C++20 及以前的功能，提到的 C++23 接口仅作延伸。

想先看纯知识追问路线，可回到 [C++ 通用高频考点](./C++通用高频考点_追问知识链QA.md)；想专攻控制块和强弱计数，可看 [手搓智能指针代码解读](./手搓智能指针代码解读.md)。
