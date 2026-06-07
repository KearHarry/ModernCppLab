// =============================================================================
//  E1 · 生成式句柄池 Slot Map（generational handle）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现游戏引擎里**最核心的对象引用机制**：用一个整数 **句柄(Handle)** 代替裸指针来
//  引用游戏对象（实体、组件、资源）。句柄 = (下标 index, 代数 generation)。对象存在
//  一个**密集数组**里，删除对象时把该槽位的"代数"+1，于是所有指向旧对象的句柄**自动失效**
//  ——再用旧句柄去访问会安全地拿到 nullptr，而不是悬空指针崩溃。这就是 EnTT、Unity DOTS、
//  各大引擎都在用的 **slot map / handle pool** 模式，是高难度游戏岗的必考题。
//
//  【为什么游戏引擎不用裸指针 / shared_ptr？】
//    · **裸指针**：对象销毁后指针悬空，别处还存着它 → use-after-free 崩溃，且无法检测。
//    · **shared_ptr**：引用计数有原子开销、控制块分散在堆上（cache 不友好）、还会因
//      循环引用泄漏；几万个对象每帧访问，开销和内存布局都不可接受。
//    · **句柄(handle)**：只是两个 32 位整数，可随意拷贝/存储/网络传输；对象集中存放在
//      连续数组里（**cache 友好**，适合批量遍历）；最关键——**能检测"我引用的对象还在不在"**。
//
//  【核心机关：generation（代数 / 版本号）怎么让旧句柄失效】
//      slots_:  [0]obj  [1]obj  [2]obj  ...        每个槽位还记一个 generation
//      插入 A → 拿到槽位 1，generation=0 → 返回句柄 {index=1, gen=0}
//      删除 A → 槽位 1 标记空闲、**generation 变 1**，下标 1 进 free_list
//      插入 B → 复用空闲槽位 1，但它的 generation 现在是 1 → 返回句柄 {index=1, gen=1}
//
//  现在你手里若还存着 A 的旧句柄 {1, 0}：去查 slots_[1].generation==1 ≠ 0 → **失效**，
//  get() 返回 nullptr。新句柄 {1, 1} 才有效。**同一个下标被复用了，但代数不同，
//  两个句柄天然区分得开**——这就是"生成式(generational)"的含义，O(1) 就能判出
//  "悬空引用"，而裸指针永远做不到。
//
//  【三个关键操作都是 O(1)】
//    insert(value): 有空闲槽就复用(从 free_list 弹一个)，否则在尾部追加；标记占用；返回句柄。
//    get(handle):   先 valid() 校验(下标在界内 + 占用中 + 代数匹配)，通过才返回 &value，否则 nullptr。
//    erase(handle): valid 才删——标记空闲、**generation++**、下标入 free_list。
//
//  【和后续模块的关系】
//  这是"数据导向设计(DOD)"的入口：对象连续存放、用整数索引、避免指针追逐。它和
//  C3 内存池、E2 帧分配器一起构成游戏引擎内存/对象管理的三件套。
//
//  【你会实现的 3 个 TODO】（valid() 已给好，是判断句柄有效性的核心不变量）
//    E1-1  insert —— 复用空闲槽或追加，标记占用，返回 {下标, 该槽当前代数}
//    E1-2  get    —— valid 才返回 &value，否则 nullptr
//    E1-3  erase  —— valid 才删：标记空闲、代数 +1、下标入 free_list
//
// =============================================================================
#pragma once

#include <cstddef>  // std::size_t
#include <cstdint>  // std::uint32_t
#include <utility>  // std::move
#include <vector>   // std::vector

namespace cppbc {

// 句柄：纯数值，可随意拷贝/存储。index 定位槽位，generation 标识"第几次使用该槽位"。
struct Handle {
    std::uint32_t index;       // 槽位下标
    std::uint32_t generation;  // 代数（版本号）

    // 一个约定俗成的"无效句柄"取值（满下标）。
    static constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;

    // C++20：默认按成员逐一比较，便于测试里比对句柄是否相等。
    friend bool operator==(const Handle&, const Handle&) = default;
};

// ---------------------------------------------------------------------------
//  SlotMap：把 T 存进密集数组，对外只发整数句柄；删除靠代数自增令旧句柄失效。
//  要求 T 可默认构造、可移动赋值（空闲槽位预留一个默认值，插入时被覆盖）。
// ---------------------------------------------------------------------------
template <class T>
class SlotMap {
    struct Slot {
        T             value{};         // 存放的对象（空闲时是默认值，占用时是真实值）
        std::uint32_t generation = 0;  // 本槽位被使用的代数
        bool          occupied   = false;
    };

    std::vector<Slot>          slots_;      // 密集存储所有槽位
    std::vector<std::uint32_t> free_list_;  // 空闲槽位下标栈（删除后回收，插入时复用）
    std::size_t                size_ = 0;   // 当前存活对象数

public:
    // 句柄是否有效：下标在界内、槽位占用中、且代数匹配。这是整个机制的不变量（已给好）。
    bool valid(Handle h) const {
        return h.index < slots_.size()
            && slots_[h.index].occupied
            && slots_[h.index].generation == h.generation;
    }

    // ===================== TODO(E1-1) insert ========================
    //  插入一个对象，返回它的句柄。优先复用空闲槽位（free_list 非空），否则尾部追加。
    //    std::uint32_t idx;
    //    if (!free_list_.empty()) {
    //        idx = free_list_.back();
    //        free_list_.pop_back();
    //    } else {
    //        idx = static_cast<std::uint32_t>(slots_.size());
    //        slots_.push_back(Slot{});            // 新槽位，generation 从 0 起
    //    }
    //    slots_[idx].value    = std::move(value);
    //    slots_[idx].occupied = true;
    //    ++size_;
    //    return Handle{idx, slots_[idx].generation};   // 代数取"该槽位当前的代数"
    // ===============================================================
    Handle insert(T value) {
        // TODO
        (void)value;
        return Handle{Handle::kInvalid, 0};   // 骨架：返回无效句柄
    }

    // ===================== TODO(E1-2) get ===========================
    //  按句柄取对象指针；句柄失效（对象已删/被复用）则返回 nullptr。
    //    if (!valid(h)) return nullptr;
    //    return &slots_[h.index].value;
    // ===============================================================
    T* get(Handle h) {
        // TODO
        (void)h;
        return nullptr;
    }
    const T* get(Handle h) const {
        if (!valid(h)) return nullptr;
        return &slots_[h.index].value;
    }

    // ===================== TODO(E1-3) erase =========================
    //  删除句柄指向的对象；成功返回 true。关键：generation++ 让所有旧句柄失效。
    //    if (!valid(h)) return false;
    //    slots_[h.index].occupied = false;
    //    ++slots_[h.index].generation;            // ★ 旧句柄从此对不上号
    //    free_list_.push_back(h.index);           // 槽位回收，待复用
    //    --size_;
    //    return true;
    // ===============================================================
    bool erase(Handle h) {
        // TODO
        (void)h;
        return false;
    }

    std::size_t size()  const noexcept { return size_; }
    bool        empty() const noexcept { return size_ == 0; }
};

} // namespace cppbc
