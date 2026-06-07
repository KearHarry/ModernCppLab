// =============================================================================
//  B3 · 原子操作与内存序 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  不用 mutex，只用 std::atomic 实现两个"无锁"小工具：
//   1) SpinLock 自旋锁：抢不到锁就"原地打转"忙等，而不是睡眠。适合临界区极短的场景。
//   2) atomic_fetch_max：把"原子地更新为更大值"做成一个函数（标准库 C++26 才有，
//      这里用 CAS 循环手写，体会无锁更新的通用套路）。
//
//  【为什么要学这个？】
//  mutex 背后其实也是原子操作 + 系统调用。面试爱问：什么是 CAS？memory_order
//  有哪几种、分别什么语义？acquire/release 怎么配对？自旋锁 vs 互斥锁怎么选？
//  写一遍自旋锁和 CAS 循环，这些就都通了。
//
//  【关键工具】
//   - std::atomic<T>        原子变量：load/store/fetch_add/compare_exchange 等操作
//                           不可被打断，多线程同时访问也不会"读到一半"。
//   - std::atomic_flag      最简单的原子布尔（test_and_set / clear），自旋锁的标配。
//   - CAS (compare_exchange) "比较并交换"：仅当当前值等于我预期的旧值时，才写入新值。
//                           是几乎所有无锁算法的基石。
//
//  【memory_order 速查（先有印象，写题时回看）】
//   - relaxed  只保证这一个变量自身的原子性，不管它和别的读写的先后 → 计数器够用。
//   - acquire  "加载屏障"：本次读之后的操作，不会被重排到这次读之前。配对 release 读。
//   - release  "存储屏障"：本次写之前的操作，不会被重排到这次写之后。配对 acquire 写。
//   - acq_rel  读改写操作（如 fetch_sub / CAS）同时具备 acquire + release。
//   - seq_cst  最强：全局单一总顺序。默认值，最安全但可能最慢。
//
//  【acquire / release 配对的直觉】
//   线程 A：先写好数据，再用 release 把"标志位"置 1；
//   线程 B：用 acquire 读到"标志位"是 1，就保证能看到 A 写好的数据。
//   ——这就是"解锁(release) / 加锁(acquire)"语义的由来。
//
// =============================================================================
#pragma once

#include <atomic>

namespace cppbc {

// =============================================================================
//  Part 1 · SpinLock —— 自旋锁
// =============================================================================
//
//  用一个 atomic_flag 表示"锁有没有被占用"：
//   - test_and_set()：把 flag 置为 true，并【返回它之前的值】。
//       · 返回 false → 之前没人占用，现在我占上了 → 抢锁成功。
//       · 返回 true  → 之前已被别人占用 → 抢锁失败，继续转圈重试。
//   - clear()：把 flag 置回 false → 释放锁。
//
//  C++20 起 atomic_flag 默认就是"清零(未占用)"状态，不再需要 ATOMIC_FLAG_INIT。
//
class SpinLock {
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;             // 锁不可拷贝
    SpinLock& operator=(const SpinLock&) = delete;

    // ===================== TODO(B3-1) lock ===========================
    // 自旋直到抢到锁：
    //   while (flag_.test_and_set(std::memory_order_acquire)) {
    //       // 抢锁失败（返回 true 表示之前已被占用），原地转圈重试。
    //       // 进阶可加 _mm_pause()/std::this_thread::yield() 降低总线压力，这里从简。
    //   }
    //   为什么用 acquire？抢到锁后进入临界区，临界区内的读写不能被重排到
    //   "加锁"之前——acquire 正是这个语义，和 unlock 的 release 配对。
    // ===============================================================
    void lock() noexcept {
        // TODO
    }

    // ===================== TODO(B3-2) unlock =========================
    // 释放锁：flag_.clear(std::memory_order_release);
    //   为什么用 release？保证临界区内的所有写，在"解锁"这一刻对
    //   下一个 acquire 到锁的线程可见。
    // ===============================================================
    void unlock() noexcept {
        // TODO
    }

    // ===================== TODO(B3-3) try_lock =======================
    // 尝试抢一次锁，不自旋：
    //   抢到返回 true，没抢到返回 false。
    //   return !flag_.test_and_set(std::memory_order_acquire);
    //   （test_and_set 返回旧值：旧值 false=没人占→我抢到了→取反返回 true）
    // ===============================================================
    bool try_lock() noexcept {
        // TODO
        return true;  // ← 占位：当前实现会"假装"总是抢到锁
    }

private:
    std::atomic_flag flag_;  // C++20：默认清零（未上锁）
};

// =============================================================================
//  Part 2 · atomic_fetch_max —— 原子地"取较大值并更新"
// =============================================================================
//
//  目标：把 target 更新为 max(target, value)，并返回【更新前】的旧值；
//        整个过程对其它线程是原子的（不会丢失并发更新）。
//
//  为什么需要 CAS 循环，而不能"if (value > target) target = value;"？
//   因为"读 target → 比较 → 写 target"这三步之间，别的线程可能已经把 target
//   改大了。CAS 把"仅当 target 还等于我读到的 cur 时，才写入 value"做成一个
//   原子动作；万一中途被别人改了，CAS 失败并把最新值塞回 cur，循环重试即可。
//   这是无锁更新的通用范式：load 一次 → 算新值 → CAS，失败就重来。
//
inline long atomic_fetch_max(std::atomic<long>& target, long value) noexcept {
    // ===================== TODO(B3-4) =====================
    // CAS 循环实现：
    //   long cur = target.load(std::memory_order_relaxed);
    //   while (value > cur) {
    //       // 仅当 target 仍等于 cur 时，把它换成 value。
    //       if (target.compare_exchange_weak(
    //               cur, value,
    //               std::memory_order_acq_rel,   // 成功：读改写，acq_rel
    //               std::memory_order_relaxed))  // 失败：仅读，relaxed
    //       {
    //           break;  // 更新成功
    //       }
    //       // CAS 失败：cur 已被自动刷新为 target 的最新值，回到 while 重新比较。
    //   }
    //   return cur;  // 返回"更新前"的值（若 value 不更大，cur 就是当前值）
    //
    //   关于 compare_exchange_weak：它在某些平台可能"伪失败"（值没变也返回
    //   false），所以必须放在循环里用；换来的是更高效。compare_exchange_strong
    //   不会伪失败，但单次开销略大——循环场景里通常用 weak。
    // =====================================================
    return target.load();  // ← 占位
}

} // namespace cppbc
