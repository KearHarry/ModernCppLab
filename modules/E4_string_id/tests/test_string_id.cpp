// =============================================================================
//  E4 测试文件 —— 检查你的编译期 FNV-1a 字符串哈希写对了没有
// -----------------------------------------------------------------------------
//  骨架阶段：三个函数都恒返回 0。测试用【运行期】EXPECT_* 校验，所以骨架能正常编译、
//  只是跑出来变红（不会因 static_assert 而编译失败）。每个用例都含一个"钉死已知值"的
//  断言，确保骨架的 0 一定触发失败。
// =============================================================================
#include "test_framework.hpp"
#include "string_id.hpp"

using namespace cppbc;

// ---- 空串：FNV-1a 的不变量——空输入哈希 == offset basis ----
TEST(E4_offset_basis, empty_hashes_to_basis) {
    EXPECT_EQ(fnv1a_32("", 0), kFnvOffsetBasis32);   // 骨架返回 0 ≠ basis → 红
}

// ---- 已知向量：用 FNV-1a 32 的标准测试向量钉死正确性 ----
TEST(E4_known_vectors, matches_canonical_fnv1a) {
    EXPECT_EQ(fnv1a_32("a", 1),       static_cast<HashId>(0xe40c292cu));
    EXPECT_EQ(fnv1a_32("foobar", 6),  static_cast<HashId>(0xbf9cf968u));
}

// ---- 区分性 & 确定性：不同串不同哈希；同串恒等 ----
TEST(E4_distinct, different_strings_differ) {
    HashId a = fnv1a_32("hello", 5);
    HashId b = fnv1a_32("world", 5);
    EXPECT_NE(a, b);                                  // 雪崩：不同输入 → 不同输出（骨架 0==0 → 红）
    EXPECT_EQ(fnv1a_32("hello", 5), a);               // 确定性：同输入恒等
    EXPECT_NE(a, static_cast<HashId>(0));             // 钉死：真实哈希非 0（骨架 → 红）
}

// ---- C 字符串重载：以 '\0' 结尾的重载与显式长度版结果一致 ----
TEST(E4_cstr_overload, null_terminated_matches_explicit_len) {
    EXPECT_EQ(fnv1a_32("hello"), fnv1a_32("hello", 5));   // strlen 版 == 显式长度版
    EXPECT_NE(fnv1a_32("hello"), static_cast<HashId>(0)); // 钉死非 0（骨架 → 红）
}

// ---- 用户自定义字面量："name"_id 等价于对名字调用核心哈希 ----
TEST(E4_udl, literal_equals_function) {
    EXPECT_EQ("foobar"_id, fnv1a_32("foobar", 6));
    EXPECT_EQ("foobar"_id, static_cast<HashId>(0xbf9cf968u));  // 钉死已知值（骨架 → 红）
}

// ---- 编译期求值：constexpr 变量能用 _id 初始化（证明编译期可算），值再用运行期校验 ----
TEST(E4_constexpr, evaluates_at_compile_time) {
    constexpr HashId idle = "idle"_id;                // 若非 constexpr 可算，这行编译不过
    constexpr HashId run  = "run"_id;
    EXPECT_NE(idle, static_cast<HashId>(0));          // 骨架 0 → 红
    EXPECT_NE(idle, run);                             // 两个状态名不同 ID（骨架 0==0 → 红）
    EXPECT_EQ(idle, fnv1a_32("idle", 4));             // 字面量与函数一致
}
