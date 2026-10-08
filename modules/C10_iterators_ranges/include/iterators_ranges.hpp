// =============================================================================
//  C10 · 迭代器、算法与 Ranges —— 可编译课程骨架
// =============================================================================
#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace cppbc {

template <class T>
class StridedIterator {
public:
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::remove_cv_t<T>;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using pointer = T*;

    StridedIterator() = default;
    StridedIterator(T* base, difference_type stride)
        : StridedIterator(base, 0, stride) {}
    StridedIterator(T* base, difference_type index, difference_type stride)
        : base_(base), index_(index), stride_(stride) {
        if (stride <= 0)
            throw std::invalid_argument("StridedIterator: stride must be positive");
    }

    reference operator*() const noexcept { return *(base_ + index_ * stride_); }
    pointer operator->() const noexcept { return base_ + index_ * stride_; }

    // TODO(C10-1)：实现前后置 ++/--，每次把逻辑下标 index_ 移动一位。
    StridedIterator& operator++() noexcept { return *this; }
    StridedIterator operator++(int) noexcept { auto old = *this; return old; }
    StridedIterator& operator--() noexcept { return *this; }
    StridedIterator operator--(int) noexcept { auto old = *this; return old; }

    // TODO(C10-2)：基于逻辑下标实现随机访问位移、差值和下标。
    StridedIterator& operator+=(difference_type) noexcept { return *this; }
    StridedIterator& operator-=(difference_type) noexcept { return *this; }
    reference operator[](difference_type) const noexcept {
        return *(base_ + index_ * stride_); // 安全占位：尚未应用下标位移。
    }

    friend StridedIterator operator+(StridedIterator it, difference_type n) noexcept {
        it += n; return it;
    }
    friend StridedIterator operator+(difference_type n, StridedIterator it) noexcept {
        it += n; return it;
    }
    friend StridedIterator operator-(StridedIterator it, difference_type n) noexcept {
        it -= n; return it;
    }
    friend difference_type operator-(StridedIterator lhs,
                                     StridedIterator rhs) noexcept {
        (void)lhs; (void)rhs;
        return 0; // TODO(C10-2)
    }

    friend bool operator==(StridedIterator lhs, StridedIterator rhs) noexcept {
        return lhs.base_ == rhs.base_ && lhs.index_ == rhs.index_
            && lhs.stride_ == rhs.stride_;
    }
    friend auto operator<=>(StridedIterator lhs, StridedIterator rhs) noexcept {
        if (lhs.base_ != rhs.base_)
            return std::less<T*>{}(lhs.base_, rhs.base_)
                ? std::strong_ordering::less : std::strong_ordering::greater;
        if (lhs.stride_ != rhs.stride_)
            return lhs.stride_ <=> rhs.stride_;
        return lhs.index_ <=> rhs.index_;
    }

private:
    T* base_ = nullptr;
    difference_type index_ = 0;
    difference_type stride_ = 1;
};

template <class T>
class StridedView {
public:
    StridedView(T* data, std::size_t count, std::ptrdiff_t stride)
        : data_(data), count_(count), stride_(stride) {
        if (stride <= 0)
            throw std::invalid_argument("StridedView: stride must be positive");
        if (count > static_cast<std::size_t>(
                        std::numeric_limits<std::ptrdiff_t>::max()))
            throw std::length_error("StridedView: count exceeds iterator range");
        if (count > 1 && count - 1 > static_cast<std::size_t>(
                                         std::numeric_limits<std::ptrdiff_t>::max()
                                         / stride))
            throw std::length_error("StridedView: strided offset exceeds iterator range");
    }

    auto begin() const noexcept { return StridedIterator<T>{data_, 0, stride_}; }

    // TODO(C10-3)：尾迭代器保存逻辑下标 count，不构造越过底层数组 one-past
    // 的物理指针；只有解引用有效位置时才计算 base + index*stride。
    auto end() const noexcept {
        // 安全占位：暂与 begin 相等，避免 C10-1 尚未实现时遍历永久不前进。
        // 完成 TODO 时把逻辑下标 0 替换为 static_cast<std::ptrdiff_t>(count_)。
        return StridedIterator<T>{data_, 0, stride_};
    }

    std::size_t size() const noexcept { return count_; }

private:
    T* data_;
    std::size_t count_;
    std::ptrdiff_t stride_;
};

// 用“投影”把对象映射到待比较字段。这是 ranges 算法的重要接口思想。
template <std::random_access_iterator It, class T, class Proj>
It projected_lower_bound(It first, It last, const T& value, Proj projection) {
    // TODO(C10-4)：二分半开区间 [first,last)，用 std::invoke 调用 projection，
    // 比较 std::invoke(projection, *mid) 与 value，从而同时支持函数对象与成员指针。
    // 必须只做 O(log n) 次比较，并返回第一个“不小于 value”的位置。
    (void)last; (void)value; (void)projection;
    return first; // 骨架：安全但通常错误
}

} // namespace cppbc
