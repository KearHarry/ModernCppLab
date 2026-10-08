# D7 · 类型转换与类型安全（static/dynamic/const/reinterpret cast）

难度 ⭐⭐⭐⭐ · 预计 3h

## 为什么大厂爱考

C 风格强转把多种语义压在一对括号里，代码能过编译却很难看出风险。面试官问四种 cast，不只是要你背用途，而是要判断：这里是否存在真实继承关系？是否需要运行期检查？底层对象到底是不是 const？这段字节流是否满足对齐、别名和生命周期规则？

真实工程里的序列化、插件接口、设备寄存器、网络协议和遗留 C API 都绕不开转换。成熟的答案不是“`reinterpret_cast` 最强”，而是把不安全转换压缩到边界，并提供失败路径与可测试不变量。

## 背景知识

### 1) `static_cast`：编译期可证明的转换

它适合数值转换、枚举与整数、`void*` 恢复原类型，以及继承图中编译期已知的方向。向上转型安全；向下转型不检查动态类型，只有你另有不变量保证时才可用。

数值收窄可能丢信息：`static_cast<int>(huge_long_long)` 不会报错。因此本模块实现 `checked_to_int`，先做范围检查，再显式转换。示例资产的 `pixels()` 同样先检查负尺寸和乘法范围，并以 `optional<int>` 表达无有效结果，避免有符号乘法溢出。

### 2) `dynamic_cast`：多态继承图中的运行期检查

源类型必须是多态类型（至少有一个虚函数）。向下/侧向转换时，运行期借 RTTI 检查真实对象：指针转换失败返回 `nullptr`，引用转换失败抛 `std::bad_cast`。这比 unchecked downcast 安全，但频繁按具体类型分支可能说明设计本该用虚函数或 `variant`。

### 3) `const_cast`：改变 cv 限定，不改变对象事实

`const_cast` 能从“指向可变对象的 const 视图”恢复写权限：底层对象原本非 const 时修改是合法的。若对象本身定义为 `const`，去 const 后写仍是未定义行为。cast 改的是表达式类型，不是物理对象属性。

### 4) `reinterpret_cast`：低层映射，不是规则豁免证

典型用途是指针与足够宽整数往返、查看地址、与平台 ABI 交互。它不会自动满足：

- 目标类型的对齐；
- strict aliasing；
- 目标对象生命周期已经开始；
- 字节序与协议格式；
- 函数指针调用约定。

从字节缓冲直接转 `uint32_t*` 解引用可能同时违反对齐和别名规则。本模块用逐字节解码。`std::bit_cast` 只适用于源/目标大小相同的 trivially-copyable 类型；由于标准不保证 `float` 恰为 32 位，示例按 `sizeof(float)` 返回字节表示，而不强转成 `uint32_t`。

### 5) 安全的对象表示视图

C++ 允许通过 `char`、`unsigned char` 或 `std::byte` 查看任意对象的对象表示。本模块把 `object_bytes` 限定为 trivially-copyable 类型，并从 `unsigned char*` 逐字节复制；反过来把任意字节直接当成任意对象仍要考虑有效表示与生命周期。

## 你要实现什么

打开 [include/casts_type_safety.hpp](include/casts_type_safety.hpp)：

| 编号 | 函数 | 任务 |
|------|------|------|
| D7-1 | `classify` | 用 `dynamic_cast` 识别两个多态派生类型 |
| D7-2 | `try_image` | 提供失败返回空指针的受检向下转换 |
| D7-3 | `checked_to_int` | 范围检查后再做 `static_cast<int>` |
| D7-4 | `increment_through_const_view` | 演示底层对象可变时的安全 `const_cast` |
| D7-5 | 指针整数往返 | 用 `uintptr_t` 保存并恢复同类型地址 |
| D7-6 | `decode_u32_le` | 不靠未对齐指针，逐字节解析小端整数 |

## 关键坑

- **`static_cast<Derived*>` 不检查真实类型**：基类指针实际指向别的派生类时，后续使用是 UB。
- **`dynamic_cast` 要求多态源类型**：没有虚函数就没有所需 RTTI 入口。
- **不要“去 const 后都能写”**：真正 const 对象仍不可修改。
- **地址对齐不等于别名合法**：即便地址是 4 的倍数，也不能凭空把 byte 数组当活着的 `uint32_t`。
- **不要猜字节序**：协议必须显式规定 endian 并逐字节/专用 API 解码。
- **函数指针不可随意互转调用**：调用约定和签名不匹配可能直接破坏栈/寄存器。
- **优先窄接口**：把不可避免的低层 cast 封装在一个已验证函数中，别散布在业务逻辑。

## 如何验证

```powershell
ctest --test-dir build -R D7 --output-on-failure
```

测试覆盖：正确/错误动态向下转换、空指针路径、安全像素乘法、整数边界与在可表示时构造的越界值、底层可变对象的 const 视图、指针整数往返、小端解码，以及长度为 `sizeof(float)` 的 `std::byte` 对象表示。

> 骨架全部返回保守的 `unknown/nullptr/nullopt/0/false`。测试预期红，但不会执行一次错误向下转型、未对齐解引用或修改真正 const 对象。

## 面试追问

1. **四种命名 cast 的核心区别？**

   **答**：`static_cast` 表达编译期已知关系/显式数值转换；`dynamic_cast` 在多态继承图中运行期校验；`const_cast` 专门改变 cv 限定；`reinterpret_cast` 表达低层地址/表示映射。拆开命名让代码审查者一眼看出风险类别，优于 C 风格 cast。

2. **`dynamic_cast` 指针和引用失败分别怎样？**

   **答**：目标为指针时返回 `nullptr`，适合正常可失败分支；目标为引用时抛 `std::bad_cast`，因为引用不能表示空。源对象需属于多态层次，向上转换通常无需 dynamic_cast。

3. **什么时候 `static_cast<Derived&>(base)` 合法？**

   **答**：只有程序通过其他可靠不变量确定 `base` 的动态类型就是 `Derived`（或其派生）时。编译器只验证继承关系，不验证真实对象；若判断错误，使用结果就是 UB。缺乏证明时用 `dynamic_cast`。

4. **`const_cast` 的合法修改场景是什么？**

   **答**：底层对象原本非 const，只是经 `const T&/T*` 视图传入，例如遗留 API 错误地漏掉 const-correctness。若对象定义为 `const T x`，即使 cast 得到 `T&`，写它仍是 UB，可能因为只读段或编译器常量传播产生诡异结果。

5. **为什么网络缓冲区不能直接 `reinterpret_cast<uint32_t*>`？**

   **答**：起始地址可能未满足 `uint32_t` 对齐，缓冲区中未必存在生命周期已开始的 `uint32_t` 对象，别名规则也可能不允许，协议字节序还可能与主机不同。用逐字节移位、`memcpy` 到本地整数再转 endian，或专用解析 API。

6. **`std::bit_cast` 与 `reinterpret_cast` 解引用有何区别？**

   **答**：`bit_cast<To>(from)` 对相同大小、trivially-copyable 类型按对象表示复制出一个新的 `To` 值，避免指针别名和对齐问题，通常编译成零开销；reinterpret 指针后解引用声称原地址处已有目标对象，要求更苛刻。

7. **strict aliasing 是什么？哪些类型可查看任意对象字节？**

   **答**：优化器可假设不相容类型的 glvalue 通常不会指向同一对象，违反假设访问会 UB。`char`、`unsigned char`、`std::byte` 被特许查看任意对象表示；这只允许读写字节表示，不代表任意字节序列都是任意类型的有效值。

8. **频繁 `dynamic_cast` 为什么可能是设计味道？**

   **答**：若业务不断判断具体派生类型再分支，行为没有放在拥有它的类型里，新增派生类还要修改中央 if 链。可考虑虚函数、visitor 或封闭集合的 `std::variant`。但插件边界、反序列化和调试工具中的偶发受检转换完全合理。
