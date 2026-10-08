// =============================================================================
//  B7 · C++ 内存模型：happens-before、发布协议、fence 与引用计数
// =============================================================================
//
//  重要说明：单元测试能验证“协议产生的功能结果”，却不能靠跑若干次证明一个弱内存序
//  算法在所有 CPU 上正确。本模块把每个协议封装成可审阅的小设施；TODO 占位不会读取
//  未发布的普通数据，因此骨架阶段没有 data race，只会确定性红测。
// =============================================================================
#pragma once

#include <atomic>
#include <cstddef>

namespace cppbc {

// 把协议要求的 memory order 提升为可被 static_assert 审阅的编译期契约。
// TODO 实现必须使用这些名字；运行若干次“看起来正常”不能替代对策略的静态检查与证明。
namespace memory_order_policy {
inline constexpr std::memory_order kPublishStore   = std::memory_order_release;
inline constexpr std::memory_order kConsumeLoad    = std::memory_order_acquire;
inline constexpr std::memory_order kFlagRelaxed    = std::memory_order_relaxed;
inline constexpr std::memory_order kReleaseFence   = std::memory_order_release;
inline constexpr std::memory_order kAcquireFence   = std::memory_order_acquire;
inline constexpr std::memory_order kRefIncrement   = std::memory_order_relaxed;
inline constexpr std::memory_order kRefDecrement   = std::memory_order_release;
inline constexpr std::memory_order kCountObserve   = std::memory_order_relaxed;
} // namespace memory_order_policy

// -----------------------------------------------------------------------------
// ReleaseAcquireMailbox：单次发布一个普通 int。
// 写 payload_ 后 release-store ready_；只有 acquire-load 读到 true 才能读 payload_。
// -----------------------------------------------------------------------------
class ReleaseAcquireMailbox {
public:
    // ===================== TODO(B7-1) release 发布 =================
    //   payload_ = value;
    //   ready_.store(true, memory_order_policy::kPublishStore);
    // release 之前的普通写将被发布给读到这个 true 的 acquire 操作。
    // ===============================================================
    void publish(int value) noexcept {
        (void)value; // 安全占位：不发布，消费者也就不会读取 payload_。
    }

    // ===================== TODO(B7-2) acquire 消费 =================
    //   if (!ready_.load(memory_order_policy::kConsumeLoad)) return false;
    //   out = payload_;
    //   return true;
    // 必须先 acquire 成功再碰普通 payload_；反过来会与发布者形成 data race。
    // ===============================================================
    bool try_consume(int& out) const noexcept {
        (void)out;
        return false;
    }

    bool ready_relaxed() const noexcept {
        return ready_.load(memory_order_policy::kFlagRelaxed);
    }

private:
    int               payload_ = 0; // 不是 atomic，完全依赖发布协议保护。
    std::atomic<bool> ready_{false};
};

// -----------------------------------------------------------------------------
// FenceMailbox：把 release/acquire 语义放到 fence 上，标志本身只用 relaxed。
// -----------------------------------------------------------------------------
class FenceMailbox {
public:
    // ===================== TODO(B7-3) release fence =================
    //   payload_ = value;
    //   std::atomic_thread_fence(memory_order_policy::kReleaseFence);
    //   state_.store(1, memory_order_policy::kFlagRelaxed);
    // ===============================================================
    void publish(int value) noexcept {
        (void)value;
    }

    // ===================== TODO(B7-4) acquire fence =================
    //   if (state_.load(memory_order_policy::kFlagRelaxed) == 0) return false;
    //   std::atomic_thread_fence(memory_order_policy::kAcquireFence);
    //   out = payload_;
    //   return true;
    // 关键：acquire fence 必须位于“读到发布标志”之后、读取 payload_ 之前。
    // ===============================================================
    bool try_consume(int& out) const noexcept {
        (void)out;
        return false;
    }

private:
    int                   payload_ = 0;
    std::atomic<unsigned> state_{0};
};

// -----------------------------------------------------------------------------
// HappensBeforeChain：A 发布数据到 stage1，B 获取后转发到 stage2，C 再获取。
// 展示 happens-before 的传递性：A -> B -> C。
// -----------------------------------------------------------------------------
class HappensBeforeChain {
public:
    // ===================== TODO(B7-5) 两段同步链 ===================
    // publish：
    //   payload_ = value;
    //   stage1_.store(true, memory_order_policy::kPublishStore);
    // relay：
    //   if (!stage1_.load(memory_order_policy::kConsumeLoad)) return false;
    //   stage2_.store(true, memory_order_policy::kPublishStore);
    //   return true;
    // consume：
    //   if (!stage2_.load(memory_order_policy::kConsumeLoad)) return false;
    //   out = payload_; return true;
    // ===============================================================
    void publish(int value) noexcept { (void)value; }
    bool relay() noexcept { return false; }
    bool try_consume(int& out) const noexcept {
        (void)out;
        return false;
    }

private:
    int               payload_ = 0;
    std::atomic<bool> stage1_{false};
    std::atomic<bool> stage2_{false};
};

// -----------------------------------------------------------------------------
// AtomicRefCount：只管理计数，不 delete 对象。release_ref() 在“应由调用者销毁对象”
// 时返回 true，因此测试能安全观察最后一次释放，而不会让骨架泄漏或悬空。
// -----------------------------------------------------------------------------
class AtomicRefCount {
public:
    AtomicRefCount() noexcept = default; // 初始由创建者持有 1 个引用。

    // ===================== TODO(B7-6) 增加引用 =====================
    // refs_.fetch_add(1, memory_order_policy::kRefIncrement);
    // 调用者已经拥有有效引用，+1 只需原子性，不负责发布对象。
    // ===============================================================
    void add_ref() noexcept {
        // 安全占位：计数保持 1。
    }

    // ===================== TODO(B7-7) 最后一次释放 =================
    // 推荐 shared_ptr 常见模式：
    //   if (refs_.fetch_sub(1, memory_order_policy::kRefDecrement) != 1) return false;
    //   std::atomic_thread_fence(memory_order_policy::kAcquireFence);
    //   return true;
    // release 把各持有者先前的写送入 release sequence；判零线程用 acquire fence
    // 汇合可见性，随后才可以执行析构。前置条件：调用次数不超过拥有的引用数。
    // ===============================================================
    bool release_ref() noexcept {
        return false; // 安全占位：不减计数，绝不会下溢。
    }

    std::size_t use_count() const noexcept {
        return refs_.load(memory_order_policy::kCountObserve);
    }

private:
    std::atomic<std::size_t> refs_{1};
};

} // namespace cppbc
