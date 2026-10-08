// =============================================================================
//  B5 · 线程池 —— 新手导读
// =============================================================================
//
//  【这个文件是干什么的？】
//  实现一个固定大小的线程池：开 N 个工作线程，谁有空谁就从任务队列里取任务来跑。
//  你用 submit(函数, 参数...) 提交任务，立刻拿到一个 std::future，将来用它取结果。
//  这是 B2 阻塞队列 + "future/异步结果" 的综合应用，几乎是所有服务端框架的标配。
//
//  【为什么不每来一个任务就 new 一个线程？】
//  创建/销毁线程很贵（要进内核、分配栈）。线程池把线程"养着"复用，任务排队等线程，
//  既省开销又能限制并发度（背压）。
//
//  【三个关键工具】
//   - std::thread             工作线程本体。
//   - std::function<void()>   "类型擦除"的可调用对象——把各种签名的任务统一装箱进队列。
//   - std::packaged_task<R()> 把"一个返回 R 的调用"打包，自带一个 std::future<R>；
//                             调用它时，返回值会自动塞进那个 future。
//   - std::future<R>          异步结果的"取货凭证"：f.get() 会等任务跑完并取回返回值。
//
//  【submit 的整体套路（背下来）】
//      把 (f, args...) 打包成 packaged_task<R()> ──► 取出它的 future ──►
//      把"调用这个 packaged_task"的动作包成 std::function<void()> 压进队列 ──►
//      notify_one 唤醒一个工作线程 ──► 把 future 返回给调用者。
//
//  【优雅关闭（graceful shutdown）】
//  析构时不能粗暴地 kill 线程，要："置停止标志 → notify_all 唤醒所有等待的线程 →
//  join 等它们把手头/队列里的活干完再退出"。否则要么丢任务，要么线程泄漏无法 join。
//  （构造函数和析构函数本模块已给出，重点体会其中的关闭握手；你来写 worker_loop 与 submit。）
//
// =============================================================================
#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppbc {

class ThreadPool {
public:
    // 启动 n 个工作线程，每个都跑 worker_loop()。已给出。
    //   （建议 n >= 1；否则提交的任务没人执行，future.get() 会一直等。）
    explicit ThreadPool(std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    // 线程池不可拷贝（持有线程与锁）。
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // ---- 优雅关闭（已给出，仔细看这三步"关闭握手"） ----
    //   1) 上锁把 stop_ 置 true：通知所有工作线程"别再等新任务了"。
    //   2) notify_all：把正卡在 cv_.wait 上睡觉的线程全部唤醒，让它们复查条件。
    //   3) join：等每个工作线程把【队列里剩余任务跑完】后自然退出，再回收。
    //      —— 之所以能"跑完剩余任务"，取决于你在 worker_loop 里的退出条件：
    //         必须是"stop_ 且 队列空"才退出，不能一看到 stop_ 就走（否则丢任务）。
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (std::thread& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    // ===================== TODO(B5-1) submit =========================
    // 提交一个任务 f(args...)，返回 std::future<返回类型> 供将来取结果。
    //
    //   返回类型 R 用 std::invoke_result_t<F, Args...> 推导（已写在签名里）。
    //
    //   实现步骤：
    //     1) 用 shared_ptr 持有一个 packaged_task<R()>，把 f 和 args 绑进去：
    //          auto task = std::make_shared<std::packaged_task<R()>>(
    //              std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    //        （为何用 shared_ptr？packaged_task 不可拷贝，而 std::function 要求可拷贝；
    //          用 shared_ptr 包一层，lambda 捕获这个指针就可拷贝了。）
    //     2) 取出 future：std::future<R> fut = task->get_future();
    //     3) 加锁，把"调用该 task"的动作压进队列（若已关闭则不该再收新任务）：
    //          {
    //              std::lock_guard<std::mutex> lk(mtx_);
    //              if (stop_) throw std::runtime_error("submit on stopped ThreadPool");
    //              tasks_.emplace([task] { (*task)(); });
    //          }
    //     4) 唤醒一个工作线程：cv_.notify_one();
    //     5) return fut;
    // ===============================================================
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;

        // ---- 占位实现：不真正排队执行，只返回一个"已就绪但值为默认值"的 future，
        //      好让骨架能编过、测试能跑出红色。实现上面的 TODO 后请整段替换掉。----
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (stop_) throw std::runtime_error("submit on stopped ThreadPool");
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();   
        return fut;
    }

private:
    // ===================== TODO(B5-2) worker_loop ====================
    // 每个工作线程的主循环：不停地取任务、执行，直到"已关闭且队列取空"才退出。
    //
    //   for (;;) {
    //       std::function<void()> task;
    //       {
    //           std::unique_lock<std::mutex> lk(mtx_);
    //           // 没任务就睡；被唤醒条件：有任务 或 已关闭。
    //           cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
    //           // 关键退出条件：只有"已关闭 且 队列已空"才结束本线程，
    //           //   保证关闭前入队的任务都能被执行完（不丢任务）。
    //           if (stop_ && tasks_.empty()) return;
    //           task = std::move(tasks_.front());
    //           tasks_.pop();
    //       }                       // ← 注意：执行任务前先解锁！
    //       task();                 // 在锁【外】执行任务，否则所有线程被一把锁串行化，
    //                               //   还可能因任务里再次 submit 造成自死锁。
    //   }
    // ===============================================================
    void worker_loop() {
        // TODO: 实现工作线程主循环（占位：空函数体 → 线程会立刻退出，
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            //       此时提交的任务无人执行，测试因此变红；不会卡死，可安全析构）。
            task();
        }
    }

    std::vector<std::thread> workers_;          // 工作线程
    std::queue<std::function<void()>> tasks_;   // 任务队列
    std::mutex mtx_;                            // 保护任务队列与 stop_
    std::condition_variable cv_;                // 队列非空 / 关闭 时唤醒工作线程
    bool stop_ = false;                         // 关闭标志
};

} // namespace cppbc
