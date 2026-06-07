// =============================================================================
//  E4 · 编译期字符串哈希 ID（constexpr FNV-1a string id）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  把"字符串名字"在**编译期**变成一个整数 ID。游戏里到处是字符串名：资源路径
//  "textures/hero.png"、动画状态 "idle"/"run"、事件名 "OnDeath"……运行时反复用字符串
//  比较/查表又慢又占内存。做法：用 **FNV-1a 哈希**把名字算成一个 32 位整数，比较整数
//  飞快、可当 switch 标签、可作 map 键。关键在**编译期就算好**（constexpr）——
//  `"idle"_id` 在编译时就是一个常数，运行时零开销。
//
//  【为什么必须是"编译期"？】
//    若运行时才哈希，每帧把 "idle" 哈希一遍纯属浪费。constexpr 让编译器在编译时把
//    `"idle"_id` 直接折叠成 0x________ 这个常数写进指令——运行时只是个立即数。
//    更妙：编译期常数能当 **switch 的 case 标签**：
//        switch (anim_name_id) { case "idle"_id: ...; case "run"_id: ...; }
//    这是字符串做不到的（switch 只接整型常量）。这套"名字→编译期整数"是 UE 的 FName、
//    各引擎资源系统、序列化 tag 的底层套路。
//
//  【FNV-1a 算法（极简，一看就会）】
//    h = offset_basis(2166136261)
//    对每个字节 b:  h = (h XOR b) * prime(16777619)        // 注意：先异或，再乘（这是 1a）
//    返回 h（32 位，乘法自然溢出取低 32 位 = 对 2^32 取模）
//  就这么两步。"1a" 指 XOR 在前、乘法在后（FNV-1 是反过来，分散性略差）。无符号溢出
//  在 C++ 里是**良定义**的回绕，正是我们要的。
//
//  【constexpr 的两个层次】
//    · 函数标 constexpr：它**既能**编译期算（喂常量时），**也能**运行时算（喂变量时）。
//    · 用户自定义字面量 operator""_id：让 `"idle"_id` 这种写法触发编译期哈希，最顺手。
//
//  【和哈希表(C4)的关系】
//    C4 是运行时哈希桶；这里是编译期把字符串压成整数 key。二者常配合：先 _id 把名字变 32 位，
//    再拿这个整数去 C4/unordered_map 里查——key 比较从"逐字符比字符串"降为"比一个 int"。
//
//  【你会实现的 3 个 TODO】（常量已给好）
//    E4-1  fnv1a_32(data, len) —— FNV-1a 核心循环（异或→乘）
//    E4-2  fnv1a_32(s)         —— 以 '\0' 结尾的 C 字符串重载：constexpr 求长 + 调用核心
//    E4-3  operator""_id       —— 用户自定义字面量，让 "name"_id 编译期出整数
//
//  ⚠ 注意：本模块的测试用**运行期** EXPECT_EQ 校验哈希值（而非 static_assert），这样
//     骨架（返回 0）能正常编译、只是跑出来变红；你实现后即变绿。
//
// =============================================================================
#pragma once

#include <cstddef>  // std::size_t
#include <cstdint>  // std::uint32_t

namespace cppbc {

// 32 位哈希 ID 类型。
using HashId = std::uint32_t;

// FNV-1a 32 位的两个魔数（已给好）。
inline constexpr HashId kFnvOffsetBasis32 = 2166136261u;  // 0x811c9dc5
inline constexpr HashId kFnvPrime32       = 16777619u;    // 0x01000193

// ===================== TODO(E4-1) fnv1a_32(data, len) ===============
//  FNV-1a 核心：对 [data, data+len) 的每个字节做"异或→乘"。
//    HashId h = kFnvOffsetBasis32;
//    for (std::size_t i = 0; i < len; ++i) {
//        h ^= static_cast<HashId>(static_cast<unsigned char>(data[i]));  // 先把字符取成 0..255
//        h *= kFnvPrime32;                                               // 再乘质数（溢出即取模）
//    }
//    return h;
// ===================================================================
constexpr HashId fnv1a_32(const char* data, std::size_t len) noexcept {
    // TODO
    (void)data; (void)len;
    return 0;   // 骨架：恒 0（注意：测试用运行期断言，故骨架仍可编译、只是变红）
}

// ===================== TODO(E4-2) fnv1a_32(s) =======================
//  以 '\0' 结尾的 C 字符串重载：编译期数出长度，再调用核心。
//    std::size_t len = 0;
//    while (s[len] != '\0') ++len;          // constexpr 版 strlen
//    return fnv1a_32(s, len);
// ===================================================================
constexpr HashId fnv1a_32(const char* s) noexcept {
    // TODO
    (void)s;
    return 0;   // 骨架：恒 0
}

// ===================== TODO(E4-3) operator""_id =====================
//  用户自定义字面量：让 "textures/hero.png"_id 在编译期算出整数 ID。
//    return fnv1a_32(s, len);
// ===================================================================
constexpr HashId operator""_id(const char* s, std::size_t len) noexcept {
    // TODO
    (void)s; (void)len;
    return 0;   // 骨架：恒 0
}

} // namespace cppbc
