// =============================================================================
//  E3 · 多播委托 / 信号-槽 Signal（multicast delegate / event）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现游戏/GUI 框架里的**事件系统**：一个 Signal（信号）可以挂上任意多个回调（槽 slot），
//  当事件发生时 emit() 一次，**所有**挂着的回调都被依次调用。发送方完全不认识接收方，
//  接收方随时可以 connect/disconnect——这就是**观察者模式**的工业级形态：
//  Qt 的 signals/slots、虚幻引擎(UE)的 Multicast Delegate、boost.signals2 都是它。
//
//  【为什么游戏引擎离不开它？—— 解耦】
//    "玩家受伤"这件事，UI 要更新血条、音效要播放、成就系统要统计、AI 要转为警戒……
//    如果让"受伤"代码直接调用这四个模块，就把它们死死焊在一起（强耦合），加一个监听者
//    就得改一次源码。改用信号：受伤处只管 on_damage.emit(hp)，谁关心谁自己 connect。
//    **发送方零依赖接收方**，模块间彻底解耦——大型项目可维护性的关键。
//
//  【核心机关：一个回调列表 + 唯一连接号(Id)】
//    内部存一串 (id, 回调) 条目：
//      connect(fn):  分配一个**单调递增、永不复用**的 Id，把 (id, fn) 存进列表，返回 Id。
//      emit(args):   遍历列表，对每个回调 fn(args...) 调用一遍（**多播/fan-out**）。
//      disconnect(id): 按 Id 找到并移除那一条；找到返回 true，否则 false。
//    Id 就是你手里的"连接凭证"，日后凭它精确断开某一个监听者。
//
//  【回调用 std::function 装（呼应 A4 类型擦除）】
//    槽的类型是 std::function<void(Args...)>，于是 lambda、函数指针、成员函数绑定、仿函数
//    都能塞进同一个列表——这正是 A4 "类型擦除"的用武之地。Signal<Args...> 是个模板，
//    Args 就是这次事件携带的参数类型（如 Signal<int> 携带一个 int）。
//
//  【Id 为什么"单调递增、不复用"？和 E1 的对照】
//    E1 的槽位下标会复用、靠 generation 区分新旧；这里更简单——Id 只增不减、删了也不回收，
//    于是天然不会"张冠李戴"，断开一个已失效的 Id 安全返回 false。代价是 Id 会一直变大
//    （64 位足够用到地老天荒）。两种思路都常见，按场景取舍。
//
//  【你会实现的 3 个 TODO】（size/empty/clear 已给好）
//    E3-1  connect    —— 分配新 Id、存入 (id, 回调)、返回 Id
//    E3-2  disconnect —— 按 Id 移除对应条目，返回是否找到
//    E3-3  emit       —— 遍历所有槽，逐个调用 fn(args...)
//
// =============================================================================
#pragma once

#include <cstddef>     // std::size_t
#include <cstdint>     // std::uint64_t
#include <functional>  // std::function
#include <utility>     // std::move
#include <vector>      // std::vector

namespace cppbc {

// ---------------------------------------------------------------------------
//  Signal<Args...>：可挂多个 void(Args...) 回调；emit 时全部触发。
// ---------------------------------------------------------------------------
template <class... Args>
class Signal {
public:
    using Slot = std::function<void(Args...)>;  // 槽：能装任何可调用物
    using Id   = std::uint64_t;                 // 连接凭证（0 约定为"无效"）

private:
    struct Entry {
        Id   id;     // 这条连接的唯一编号
        Slot fn;     // 回调本体
    };

    std::vector<Entry> slots_;       // 所有已连接的槽
    Id                 next_id_ = 1; // 下一个要分配的 Id（从 1 起，0 留作无效）

public:
    // ===================== TODO(E3-1) connect =======================
    //  注册一个回调，返回它的唯一连接号（日后用它 disconnect）。
    //    Id id = next_id_++;                       // 取号，计数器自增（不复用旧号）
    //    slots_.push_back(Entry{id, std::move(fn)});
    //    return id;
    // ===============================================================
    Id connect(Slot fn) {
        // TODO
        (void)fn;
        return 0;   // 骨架：返回无效连接号，且不真正挂上
    }

    // ===================== TODO(E3-2) disconnect ====================
    //  按连接号移除一个回调；找到并移除返回 true，否则 false。
    //    for (std::size_t i = 0; i < slots_.size(); ++i) {
    //        if (slots_[i].id == id) {
    //            slots_.erase(slots_.begin() + static_cast<std::ptrdiff_t>(i));
    //            return true;
    //        }
    //    }
    //    return false;
    // ===============================================================
    bool disconnect(Id id) {
        // TODO
        (void)id;
        return false;   // 骨架：恒未找到
    }

    // ===================== TODO(E3-3) emit ==========================
    //  触发事件：把参数广播给当前所有已连接的回调（多播/fan-out）。
    //    for (const Entry& e : slots_) {
    //        e.fn(args...);
    //    }
    // ===============================================================
    void emit(Args... args) const {
        // TODO
        ((void)args, ...);   // 骨架：把参数标记为"已使用"，避免告警；什么也不广播
    }

    std::size_t size()  const noexcept { return slots_.size(); }   // 已连接槽数（已给好）
    bool        empty() const noexcept { return slots_.empty(); }  // 是否无任何连接（已给好）
    void        clear()       noexcept { slots_.clear(); }         // 断开全部（已给好）
};

} // namespace cppbc
