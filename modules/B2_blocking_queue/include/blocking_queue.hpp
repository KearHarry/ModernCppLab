// =============================================================================
//  B2 · 阻塞队列 / 生产者-消费者 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个"有界阻塞队列"：多个生产者线程往里放数据、多个消费者线程取数据。
//   - 队列满了：生产者 push 时自动"睡着"等，直到有空位。
//   - 队列空了：消费者 pop 时自动"睡着"等，直到有数据。
//  这就是几乎所有线程池/消息队列的核心。
//
//  【关键工具】
//   - std::mutex            互斥锁，保护共享数据(队列本身)。
//   - std::lock_guard       RAII 锁：构造加锁、析构解锁(简单场景)。
//   - std::unique_lock      更灵活的 RAII 锁，能中途解锁，【配条件变量用】。
//   - std::condition_variable  条件变量：让线程睡眠等待某条件成立，再被唤醒。
//
//  【条件变量的标准用法（背下来）】
//      std::unique_lock<std::mutex> lk(mtx_);
//      cv.wait(lk, [this]{ return 条件; });   // 条件不成立就(释放锁并)睡，醒来复查
//      // 走到这里：一定持有锁、且条件成立
//      ...改数据...
//      cv.notify_one();   // 唤醒一个等待者
//
//  【为什么 wait 要带那个 lambda 谓词？】
//  防"虚假唤醒"：线程可能没人通知也会醒。带谓词 = 醒来后自动复查，不满足就接着睡。
//
// =============================================================================
#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

namespace cppbc {

template <class T>
class BlockingQueue {
public:
    // 创建一个最多容纳 capacity 个元素的队列。
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {}

    // 队列不该被拷贝（里面有 mutex，且语义上也不该复制一份）。
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    // ===================== TODO(B2-1) push =======================
    // 放入一个元素：
    //   1) 用 unique_lock 加锁。
    //   2) 用条件变量 not_full_ 等待，直到"有空位 或 已关闭"：
    //        not_full_.wait(lk, [this]{ return q_.size() < capacity_ || closed_; });
    //   3) 若已关闭(closed_)，放弃入队，直接 return（不再接收新数据）。
    //   4) 入队：q_.push(std::move(value));
    //   5) 唤醒一个消费者：not_empty_.notify_one();
    //      （可以先 lk.unlock() 再 notify，减少"唤醒了却抢不到锁"的概率；锁内 notify 也对）
    // =============================================================
    void push(T value) {
        // TODO
        (void)value;
    }

    // ===================== TODO(B2-2) pop ========================
    // 取出一个元素到 out：
    //   1) 用 unique_lock 加锁。
    //   2) 等待，直到"有数据 或 已关闭"：
    //        not_empty_.wait(lk, [this]{ return !q_.empty() || closed_; });
    //   3) 若此时队列仍为空（说明是被 close 唤醒、且没有残留数据），return false。
    //   4) 否则取队首：out = std::move(q_.front()); q_.pop();
    //   5) 唤醒一个生产者：not_full_.notify_one();
    //   6) return true;
    //   要点：先取完残留数据，只有"已关闭且为空"才返回 false，保证不丢数据。
    // =============================================================
    bool pop(T& out) {
        // TODO
        (void)out;
        return false;
    }

    // ===================== TODO(B2-3) close ======================
    // 关闭队列并唤醒所有等待者（否则它们会永远睡着，无法 join）：
    //   1) 加锁，置 closed_ = true（用 lock_guard 即可）。
    //   2) 解锁后 not_empty_.notify_all(); not_full_.notify_all();
    // =============================================================
    void close() {
        // TODO
    }

    // ---- 下面是辅助查询函数，已给出 ----

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return q_.size();
    }
    bool closed() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return closed_;
    }

private:
    mutable std::mutex mtx_;            // mutable：const 成员函数里也能加锁
    std::condition_variable not_empty_; // 队列非空时唤醒消费者
    std::condition_variable not_full_;  // 队列非满时唤醒生产者
    std::queue<T> q_;
    std::size_t capacity_;
    bool closed_ = false;
};

} // namespace cppbc
