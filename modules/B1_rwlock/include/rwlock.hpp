// =============================================================================
//  B1 · 自己实现读写锁 RWLock（std::shared_mutex 的微缩版）—— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一把**读写锁**：允许**多个读者同时**持锁（并发读无害），但**写者必须独占**
//  （写时谁都不能进）。适用于"读多写少"的共享数据——配置表、路由表、缓存元信息……
//  比一把普通互斥锁（读也互斥、白白串行化）吞吐高得多。它就是 std::shared_mutex
//  的核心；理解它 = 理解 condition_variable 的"按条件等待/唤醒"。
//
//  【两种锁，四个动作】
//      读者： lock_shared() / unlock_shared()   —— 可与其他读者共存
//      写者： lock()        / unlock()          —— 独占，排斥一切读者与写者
//
//  【怎么用一把互斥锁 + 一个条件变量实现？】
//  内部用三个计数描述当前状态，并用 mtx_ **短暂**保护它们、用 cv_ 挂起等待：
//      int  readers_          活跃读者数
//      bool writer_           是否有写者正持锁
//      int  waiting_writers_  正在排队的写者数（用于"写者优先"，防写者饿死）
//  · lock_shared：等到「没有写者活跃，且没有写者在排队」→ ++readers_。
//      （"没有写者排队"这一条让新读者给等待的写者让路，避免写者被读者流持续插队饿死。）
//  · unlock_shared：--readers_；归零时 notify_all（可能轮到写者）。
//  · lock：++waiting_writers_；等到「没有写者、且 readers_==0」→ 置 writer_=true、--waiting。
//  · unlock：writer_=false；notify_all（唤醒读者或下一个写者）。
//  关键点：mtx_ 只在改这几个计数的瞬间持有；真正的"等待"靠 cv_.wait(lk, 谓词) 完成——
//  它会原子地"放锁+挂起"，被唤醒后"重新拿锁+复查谓词"，所以不会漏唤醒、不会忙等。
//
//  【你会实现的 4 个 TODO】
//    B1-1  lock_shared     —— 按谓词等待，然后 ++readers_
//    B1-2  unlock_shared   —— --readers_，必要时 notify_all
//    B1-3  lock            —— 登记等待、按谓词等待，置 writer_
//    B1-4  unlock          —— 清 writer_，notify_all
//
//  ⚠ 真实 std::shared_mutex 还要处理可重入策略、try_lock、超时、读写公平性可调等；本模块
//     聚焦"多读单写 + cv 条件等待"的最小正确实现（采用写者优先）。
//
// =============================================================================
#pragma once

#include <condition_variable>
#include <mutex>

namespace cppbc {

class RWLock {
    std::mutex              mtx_;              // 只用来短暂保护下面几个计数
    std::condition_variable cv_;
    int  readers_         = 0;                 // 活跃读者数
    bool writer_          = false;             // 是否有写者持锁
    int  waiting_writers_ = 0;                 // 排队中的写者数（写者优先用）

public:
    RWLock() = default;
    RWLock(const RWLock&)            = delete; // 锁不可拷贝/移动
    RWLock& operator=(const RWLock&) = delete;

    // ===================== TODO(B1-1) lock_shared ===================
    //  等到「无写者活跃 且 无写者排队」，再让自己成为一个活跃读者。
    //    std::unique_lock<std::mutex> lk(mtx_);
    //    cv_.wait(lk, [&]{ return !writer_ && waiting_writers_ == 0; });
    //    ++readers_;
    // ===============================================================
    void lock_shared() {
        // TODO（骨架：先退化成"独占"——读也串行化；并发读用例会红，但安全不崩）
        mtx_.lock();
    }

    // ===================== TODO(B1-2) unlock_shared =================
    //  退出读临界区；当最后一个读者离开时，唤醒可能在等的写者。
    //    std::unique_lock<std::mutex> lk(mtx_);
    //    if (--readers_ == 0) cv_.notify_all();
    // ===============================================================
    void unlock_shared() {
        // TODO
        mtx_.unlock();
    }

    // ===================== TODO(B1-3) lock（写者）===================
    //  登记为等待中的写者；等到「无写者 且 无读者」，再独占。
    //    std::unique_lock<std::mutex> lk(mtx_);
    //    ++waiting_writers_;
    //    cv_.wait(lk, [&]{ return !writer_ && readers_ == 0; });
    //    --waiting_writers_;
    //    writer_ = true;
    // ===============================================================
    void lock() {
        // TODO（骨架：退化成独占锁）
        mtx_.lock();
    }

    // ===================== TODO(B1-4) unlock（写者）=================
    //  释放写锁，唤醒所有等待者（读者们或下一个写者去抢）。
    //    std::unique_lock<std::mutex> lk(mtx_);
    //    writer_ = false;
    //    cv_.notify_all();
    // ===============================================================
    void unlock() {
        // TODO
        mtx_.unlock();
    }

    // ---- RAII 守卫：构造上锁、析构解锁，异常安全。已给好 ----
    class ReadGuard {
        RWLock& l_;
    public:
        explicit ReadGuard(RWLock& l) : l_(l) { l_.lock_shared(); }
        ~ReadGuard() { l_.unlock_shared(); }
        ReadGuard(const ReadGuard&)            = delete;
        ReadGuard& operator=(const ReadGuard&) = delete;
    };
    class WriteGuard {
        RWLock& l_;
    public:
        explicit WriteGuard(RWLock& l) : l_(l) { l_.lock(); }
        ~WriteGuard() { l_.unlock(); }
        WriteGuard(const WriteGuard&)            = delete;
        WriteGuard& operator=(const WriteGuard&) = delete;
    };
};

} // namespace cppbc
