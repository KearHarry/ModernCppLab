// =============================================================================
//  A1 · 移动语义与完美转发 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  这是你的"作业答案"文件。你要在这里填空（TODO），让测试通过。
//
//  【header-only 是什么意思？】
//  所有代码都写在这个 .hpp 头文件里，没有单独的 .cpp。
//  测试文件 #include 它之后，编译器会把这里的代码一起编译进去。
//
//  【本作业分两大块】
//  1) Probe + relay()  —— 学习"完美转发"（参数怎么原样传给另一个函数）
//  2) Buffer           —— 学习"Rule of Five"（类自己管理内存时要写 5 个函数）
//
//  【你需要先知道的几个词】
//  - 左值 (lvalue)：有名字、能长期存在的变量，比如 int x = 1; 里的 x
//  - 右值 (rvalue)：临时的、马上就不要的值，比如 1+2 的结果、Buffer{1,2,3}
//  - 堆内存：用 new 在"堆"上申请的空间，必须手动 delete[] 释放，否则泄漏
//  - 指针 (int*)：保存"某块内存的地址"，像门牌号，通过它能找到真正的数据
//
// =============================================================================
#pragma once   // 防止同一个头文件被重复 include 两次（否则会重复定义报错）

#include <cstddef>          // std::size_t：无符号整数，常用来表示"个数/下标"
#include <initializer_list> // 支持 Buffer{1, 2, 3} 这种写法
#include <utility>          // std::move, std::forward

// namespace：把名字包进一个"命名空间"，避免和别人的类名冲突。
// 测试里会写 using namespace cppbc; 这样就能直接用 Probe、Buffer。
namespace cppbc {

// =============================================================================
//  第一部分：Probe —— 一个"探测器"，用来观察发生了拷贝还是移动
// =============================================================================
//
//  【为什么要造这个类？】
//  拷贝和移动在结果上有时看起来一样（都得到了一个 Probe 对象），
//  但调用的函数不同。Probe 在构造时会把"我是怎么来的"记到 origin 里。
//
//  【struct 和 class 几乎一样】
//  默认 struct 成员是 public，class 默认是 private。这里用 struct 方便理解。
//
struct Probe {
    // enum class：一组命名的常量。Origin 表示"这个对象是怎么创建出来的"。
    enum class Origin {
        Default,  // 默认构造（没走拷贝也没走移动）
        Copy,     // 拷贝构造或拷贝赋值
        Move      // 移动构造或移动赋值
    };

    // 成员变量：每个 Probe 对象都有一份 origin，默认是 Default。
    Origin origin = Origin::Default;

    // ---- 下面 4 个函数是"特殊成员函数"，编译器在特定场景会自动调用 ----

    // 默认构造：Probe p; 时调用，origin 保持 Default。
    Probe() = default;

    // 拷贝构造：用另一个 Probe 创建新对象时调用。
    // 参数是 const Probe&（常量引用），表示"只读地借用对方，不搬走对方"。
    // 例：Probe a; Probe b = a;  // 这里 b 是拷贝出来的
    Probe(const Probe&) { origin = Origin::Copy; }

    // 移动构造：用"快要销毁的临时对象"创建新对象时调用。
    // 参数是 Probe&&（右值引用），表示"对方是临时的，可以掏空"。
    // noexcept：承诺这个函数不会抛异常（后面 Buffer 也会用到，很重要）。
    // 例：Probe b = Probe{};  或  Probe b = std::move(a);
    Probe(Probe&&) noexcept { origin = Origin::Move; }

    // 拷贝赋值：已经存在的对象，用另一个对象的内容覆盖自己。
    // 例：Probe a, b;  b = a;  // 注意是 = 赋值，不是构造
    Probe& operator=(const Probe&) {
        origin = Origin::Copy;
        return *this;   // 返回自己，才能链式写 a = b = c;
    }

    // 移动赋值：用临时对象的内容"搬"到自己身上。
    Probe& operator=(Probe&&) noexcept {
        origin = Origin::Move;
        return *this;
    }
};

// =============================================================================
//  TODO(A1-1)：relay() —— 完美转发
// =============================================================================
//
//  【这个函数要干什么？】
//  接收一个参数，原封不动地（保持左值/右值性质）传给 Probe 的构造函数，
//  然后返回新创建的 Probe。
//
//  【template <class T> 是什么？】
//  函数模板：T 是"类型占位符"，编译器会根据你传入的参数自动推断 T 是什么。
//  类似"这个函数能接很多种类型"，这里主要是接 Probe 的左值或右值。
//
//  【T&& 是什么？（转发引用）】
//  在模板函数里，T&& 不是普通的"右值引用"，而是"转发引用"：
//    - 你传左值  → 编译器把 T 推断成 Probe&   → T&& 变成 Probe&  （还是左值）
//    - 你传右值  → 编译器把 T 推断成 Probe    → T&& 变成 Probe&& （右值）
//
//  【关键陷阱：形参 value 有名字，在函数体内永远是左值！】
//  即使你传进来的是右值，一进函数变成 value 这个变量后，它就"有名字"了，
//  在 C++ 规则里算左值。所以直接写 Probe(value) 永远走拷贝构造。
//
//  【std::forward<T>(value) 做什么？】
//  把 value "还原"成调用者当初传入时的值类别：
//    - 当初传左值 → forward 产出左值 → 调用拷贝构造
//    - 当初传右值 → forward 产出右值 → 调用移动构造
//
//  【为什么不能用 std::move(value)？】
//  move 无条件把东西变成右值。左值传进来也会被 move 成右值 → 错误地走移动。
//  口诀：move 是"我不管你原来是什么，一律当右值"；
//       forward 是"你原来是什么，我还原成什么"。
//
template <class T>
Probe relay(T&& value) {
    return Probe(std::forward<T>(value));
}

// =============================================================================
//  第二部分：Buffer —— 管理堆上的 int 数组（像简化版 vector<int>）
// =============================================================================
//
//  【Buffer 里有什么？】
//  - data_：指向堆上 int 数组的指针（门牌号）
//  - size_：数组里有多少个 int
//
//  【为什么要自己写拷贝/移动/析构？】
//  编译器默认的拷贝只会"复制指针的值"（浅拷贝），两个 Buffer 会指向同一块内存，
//  析构时 delete 两次 → 程序崩溃（double free）。
//  所以谁用了 new[]，谁就要负责写清楚"怎么拷贝、怎么移动、怎么释放"。
//  这叫 Rule of Five（五法则）：析构、拷贝构造、移动构造、拷贝赋值、移动赋值。
//
//  【生活类比】
//  - 拷贝 = 复印：原稿还在，复印件是全新的一张纸，内容一样但互不影响
//  - 移动 = 交钥匙：把房子的钥匙交给别人，你不再拥有那套房，你的地址本要清空
//
class Buffer {
public:
    // 默认构造：空 Buffer，不占用堆内存。
    // data_ = nullptr, size_ = 0（成员声明处已有默认值）。
    Buffer() noexcept = default;

    // 构造 n 个 int，全部初始化为 0。
    // explicit：禁止 Buffer x = 10; 这种隐式转换，必须 Buffer x(10);
    // new int[n]()：在堆上申请 n 个 int，末尾 () 表示值初始化（变成 0）。
    // n 为 0 时不申请，data_ 保持 nullptr。
    explicit Buffer(std::size_t n)
        : data_(n ? new int[n]() : nullptr), size_(n) {}

    // 用 {1, 2, 3} 初始化。initializer_list 让你能写 Buffer{1, 2, 3}。
    Buffer(std::initializer_list<int> il)
        : data_(il.size() ? new int[il.size()] : nullptr), size_(il.size()) {
        std::size_t i = 0;
        for (int v : il) {          // 范围 for：遍历列表里每个数
            data_[i++] = v;         // 填进堆数组，i 自增
        }
    }

    // ---- Rule of Five：下面 5 个是你要实现的核心（本作业 A1-2 ~ A1-6）----

    Buffer(const Buffer& other);                // 拷贝构造
    Buffer(Buffer&& other) noexcept;            // 移动构造
    Buffer& operator=(const Buffer& other);     // 拷贝赋值
    Buffer& operator=(Buffer&& other) noexcept; // 移动赋值
    ~Buffer();                                  // 析构

    // ---- 访问器：读/写数据的小工具，已写好不用改 ----

    std::size_t size()  const noexcept { return size_; }
    bool        empty() const noexcept { return size_ == 0; }
    int*        data()        noexcept { return data_; }
    const int*  data()  const noexcept { return data_; }
    int&        operator[](std::size_t i)       { return data_[i]; }
    const int&  operator[](std::size_t i) const { return data_[i]; }

private:
    int*        data_ = nullptr;  // 堆数组地址；空的时候用 nullptr（空指针）
    std::size_t size_ = 0;        // 元素个数
};

// =============================================================================
//  A1-2 拷贝构造 —— 深拷贝（复印一份全新的数组）
// =============================================================================
//
//  触发时机：Buffer b = a;  或  Buffer b(a);  （a 是已存在的 Buffer）
//
//  步骤：
//  1. 复制长度 size_
//  2. 如果长度 > 0，new 一块新数组（和 other 一样大）
//  3. for 循环逐个复制元素
//
//  结果：a 和 b 各有一块独立内存，改 b 不会影响 a。
//
inline Buffer::Buffer(const Buffer& other) {
    size_ = other.size_;
    data_ = size_ ? new int[size_] : nullptr;
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

// =============================================================================
//  A1-3 移动构造 —— 偷指针 + 把源对象清空（交钥匙）
// =============================================================================
//
//  触发时机：Buffer b = std::move(a);  或  Buffer b = Buffer{1,2,3};
//            （后者临时对象会绑定到右值引用）
//
//  步骤：
//  1. 把 other 的 data_、size_ 直接拿过来（不 new，不 for 循环，所以很快 O(1)）
//  2. 把 other.data_ 设为 nullptr，other.size_ 设为 0
//
//  【为什么必须清空 other？】
//  other 迟早会析构，析构里会 delete[] data_。
//  如果不置空，other 析构时会删掉我们已经偷走的那块内存 → 崩溃。
//
inline Buffer::Buffer(Buffer&& other) noexcept {
    size_ = other.size_;
    data_ = other.data_;
    other.data_ = nullptr;
    other.size_ = 0;
}

// =============================================================================
//  A1-4 拷贝赋值 —— 先释放自己，再深拷贝对方
// =============================================================================
//
//  触发时机：Buffer a, b;  b = a;  （b 已经存在，用 = 覆盖）
//
//  和拷贝构造的区别：赋值时左边对象已经存在，可能已有自己的堆内存，
//  必须先 delete[] 掉，否则旧内存泄漏。
//
//  【自赋值 a = a 为什么要特殊处理？】
//  如果不判断，先 delete[] 自己的 data_，再拷贝自己的 data_，数据已经被删了。
//  this 是当前对象的地址，&other 是对方地址，相等说明是同一个对象。
//
inline Buffer& Buffer::operator=(const Buffer& other) {
    if (this == &other) {
        return *this;   // 自己赋给自己，啥也不做
    }
    delete[] data_;     // 释放自己原来的堆内存
    size_ = other.size_;
    data_ = size_ ? new int[size_] : nullptr;
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
    return *this;
}

// =============================================================================
//  A1-5 移动赋值 —— 释放自己，再偷对方的资源
// =============================================================================
//
//  触发时机：Buffer a, b;  b = std::move(a);
//
//  注意：b 原来可能有自己的内存（比如 b{9,9}），要先 delete[] 再偷 a 的。
//
inline Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    delete[] data_;
    size_ = other.size_;
    data_ = other.data_;
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
}

// =============================================================================
//  A1-6 析构 —— 对象销毁时自动调用，释放堆内存
// =============================================================================
//
//  触发时机：Buffer 离开作用域、被 delete、容器销毁元素时……
//
//  delete[] data_：释放 new[] 申请的数组。
//  delete[] nullptr 是安全的，什么都不做。
//  后面把指针和大小置零是良好习惯（对象已销毁，成员不应再指向有效内存）。
//
inline Buffer::~Buffer() {
    delete[] data_;
    data_ = nullptr;
    size_ = 0;
}

} // namespace cppbc
