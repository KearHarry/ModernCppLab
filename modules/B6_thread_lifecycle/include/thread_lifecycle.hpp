// =============================================================================
//  B6 · 线程生命周期、协作取消与死锁避免 —— TODO 骨架
// =============================================================================
//
//  本模块把“能启动线程”推进到“能可靠地结束线程”：
//    1) JoiningThread 用 RAII 保证 std::thread 在析构前 join；
//    2) CooperativeCounter 演示 std::jthread 自动传入 stop_token；
//    3) StoppableQueue 用 condition_variable_any 的 stop_token 重载实现可取消等待；
//    4) transfer 用 std::scoped_lock 一次锁住两把 mutex，避免 ABBA 死锁。
//
//  骨架的 TODO 都采用安全桩：不启动后台线程、不阻塞、不改共享状态。因此开箱测试只会
//  失败，不会触发 std::terminate、数据竞争或永久等待。
// =============================================================================
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>

namespace cppbc {

// -----------------------------------------------------------------------------
// JoiningThread：拥有一个 std::thread，并在析构时等待它结束。
// -----------------------------------------------------------------------------
class JoiningThread {
public:
    JoiningThread() noexcept = default;

    // ===================== TODO(B6-1) 启动线程 =====================
    // 用完美转发把函数和参数交给 thread_：
    //   template <class F, class... Args>
    //   explicit JoiningThread(F&& f, Args&&... args)
    //       : thread_(std::forward<F>(f), std::forward<Args>(args)...) {}
    //
    // 注意：成员 thread_ 必须在对象构造期间直接取得所有权；不要创建局部线程后 detach。
    // ===============================================================
    template <class F, class... Args>
        requires (!std::is_same_v<std::remove_cvref_t<F>, JoiningThread>)
    explicit JoiningThread(F&& f, Args&&... args) {
        // 安全占位：线程保持 not-joinable，析构不会 terminate。
        (void)f;
        (void)sizeof...(args);
    }

    ~JoiningThread() {
        // 安全兜底已给出：即使学员只完成 B6-1，异常离开作用域也能 join。
        if (thread_.joinable()) thread_.join();
    }

    JoiningThread(const JoiningThread&)            = delete;
    JoiningThread& operator=(const JoiningThread&) = delete;
    JoiningThread(JoiningThread&&)                 = delete;
    JoiningThread& operator=(JoiningThread&&)      = delete;

    bool joinable() const noexcept { return thread_.joinable(); }

    // 显式等待是幂等友好的：已经 join 或从未启动时什么也不做。
    void join() {
        if (thread_.joinable()) thread_.join();
    }

private:
    std::thread thread_;
};

// -----------------------------------------------------------------------------
// CooperativeCounter：给 std::jthread 使用的可停止任务。
// -----------------------------------------------------------------------------
struct CooperativeCounter {
    // ===================== TODO(B6-2) 协作式停止 ===================
    // 在循环中定期检查 st.stop_requested()，未请求停止时递增 ticks，并 yield：
    //   while (!st.stop_requested()) {
    //       ticks.fetch_add(1, std::memory_order_relaxed);
    //       std::this_thread::yield();
    //   }
    // “停止请求”不是强杀线程；任务必须主动到达检查点并返回。
    // ===============================================================
    void operator()(std::stop_token st, std::atomic<int>& ticks) const noexcept {
        (void)st;
        (void)ticks; // 安全占位：立即返回。
    }
};

// -----------------------------------------------------------------------------
// StoppableQueue：展示 stop_token + condition_variable_any 的谓词等待。
// -----------------------------------------------------------------------------
template <class T>
class StoppableQueue {
public:
    StoppableQueue() = default;
    StoppableQueue(const StoppableQueue&)            = delete;
    StoppableQueue& operator=(const StoppableQueue&) = delete;

    void push(T value) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            q_.push(std::move(value));
        }
        cv_.notify_one();
    }

    // ===================== TODO(B6-3) 可取消等待 ===================
    //   std::unique_lock<std::mutex> lk(mtx_);
    //   bool ready = cv_.wait(lk, st, [this] { return !q_.empty(); });
    //   if (!ready) return false;          // stop 被请求且队列仍为空
    //   out = std::move(q_.front());
    //   q_.pop();
    //   return true;
    //
    // 使用带谓词的重载同时抵御虚假唤醒与“检查停止后才睡下”的丢唤醒窗口。
    // ===============================================================
    bool wait_pop(std::stop_token st, T& out) {
        (void)st;
        (void)out;
        return false; // 安全占位：不等待，因此骨架绝不会卡住。
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return q_.size();
    }

private:
    mutable std::mutex          mtx_;
    std::condition_variable_any cv_;
    std::queue<T>               q_;
};

struct Account {
    explicit Account(int initial = 0) : balance(initial) {}

    mutable std::mutex mtx;
    int                balance = 0;
};

// ===================== TODO(B6-4) 多锁死锁避免 =====================
// 从 from 向 to 转账 amount：
//   if (&from == &to || amount < 0) return amount >= 0;
//   std::scoped_lock lock(from.mtx, to.mtx);  // std::lock 算法，避免 ABBA
//   if (from.balance < amount) return false;
//   from.balance -= amount;
//   to.balance   += amount;
//   return true;
//
// 不要写成 lock(from) 后 lock(to)：另一个线程若反向加锁就可能形成循环等待。
// ===================================================================
inline bool transfer(Account& from, Account& to, int amount) {
    (void)from;
    (void)to;
    (void)amount;
    return false; // 安全占位：不拿锁、不改余额。
}

// 一致地读取两个账户；正确实现 transfer 后，可用它安全观察总额。
inline int total_balance(const Account& a, const Account& b) {
    if (&a == &b) {
        std::lock_guard<std::mutex> lk(a.mtx);
        return a.balance;
    }
    std::scoped_lock lock(a.mtx, b.mtx);
    return a.balance + b.balance;
}

} // namespace cppbc
