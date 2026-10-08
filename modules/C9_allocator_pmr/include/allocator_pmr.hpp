// =============================================================================
//  C9 · Allocator / allocator_traits / PMR —— 可编译课程骨架
// =============================================================================
#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <type_traits>

namespace cppbc {

struct AllocationStats {
    std::size_t allocation_calls = 0;
    std::size_t deallocation_calls = 0;
    std::size_t bytes_allocated = 0;
    std::size_t bytes_outstanding = 0;
};

// 一个“有状态”的标准分配器。所有 rebind 后的 allocator 共享同一份统计数据，
// 因而既能喂给 vector<T>，也能观察 list 内部节点类型的分配。
template <class T>
class CountingAllocator {
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::false_type;

    CountingAllocator() : stats_(std::make_shared<AllocationStats>()) {}
    explicit CountingAllocator(std::shared_ptr<AllocationStats> stats)
        : stats_(std::move(stats)) {}

    template <class U>
    CountingAllocator(const CountingAllocator<U>& other) noexcept
        : stats_(other.stats()) {}

    // TODO(C9-1)：分配 n 个 T 的“生内存”，并更新调用次数、累计字节数和在途字节数。
    // 统计更新应在上游分配成功后提交；分配抛异常时不能留下虚假的统计值。
    [[nodiscard]] T* allocate(std::size_t n) {
        return std::allocator<T>{}.allocate(n); // 骨架：安全分配，但尚不记账
    }

    // TODO(C9-2)：把内存交还给标准分配器，并对称更新释放次数与在途字节数。
    void deallocate(T* ptr, std::size_t n) noexcept {
        std::allocator<T>{}.deallocate(ptr, n); // 骨架：安全释放，但尚不记账
    }

    std::shared_ptr<AllocationStats> stats() const noexcept { return stats_; }

    template <class U>
    bool operator==(const CountingAllocator<U>& rhs) const noexcept {
        return stats_.get() == rhs.stats().get();
    }

    template <class U>
    bool operator!=(const CountingAllocator<U>& rhs) const noexcept {
        return !(*this == rhs);
    }

private:
    template <class>
    friend class CountingAllocator;
    std::shared_ptr<AllocationStats> stats_;
};

// PMR 的运行时多态版本。容器只保存 memory_resource*，资源策略可在运行时替换。
class CountingResource final : public std::pmr::memory_resource {
public:
    explicit CountingResource(
        std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
        : upstream_(upstream) {}

    const AllocationStats& stats() const noexcept { return stats_; }

private:
    // TODO(C9-3)：把 bytes/alignment 原样转发给 upstream_；成功后再更新统计。
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        return upstream_->allocate(bytes, alignment); // 骨架：转发但不记账
    }

    // TODO(C9-4)：用完全相同的 bytes/alignment 归还内存，并更新统计。
    void do_deallocate(void* ptr, std::size_t bytes,
                       std::size_t alignment) override {
        upstream_->deallocate(ptr, bytes, alignment); // 骨架：转发但不记账
    }

    // 计数资源带有身份：只有同一个资源对象才可互相释放其内存。
    bool do_is_equal(const std::pmr::memory_resource& other) const
        noexcept override {
        return this == &other;
    }

    std::pmr::memory_resource* upstream_;
    AllocationStats stats_{};
};

} // namespace cppbc

