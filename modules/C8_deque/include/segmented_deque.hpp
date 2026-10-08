// =============================================================================
//  C8 · 分段存储 Deque —— TODO 骨架
// =============================================================================
//
//  与 vector 的单块连续内存不同，deque 把元素放进多个固定大小 Block，再用一张
//  “map（块指针表）”定位这些 Block：
//
//      map_: [空][Block*][Block*][Block*][空]
//                        first_slot_ ↑，first_offset_ 指向首元素在首块中的偏移
//
//  逻辑下标 i -> global=first_offset_+i -> 块增量 global/BlockSize、块内偏移
//  global%BlockSize。扩张 map 时只搬 Block*，不会搬 T，所以已有元素地址保持稳定。
//
//  骨架的插入/删除均为安全空操作。operator[] 保持和标准容器一样的非空前置条件，
//  测试在使用前会先 ASSERT size，因此开箱只红测、不越界。
// =============================================================================
#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cppbc {

template <class T, std::size_t BlockSize = 4>
class SegmentedDeque {
    static_assert(BlockSize > 0, "SegmentedDeque 的 BlockSize 必须大于 0");

    struct Block {
        alignas(T) std::byte bytes[sizeof(T) * BlockSize];

        T* address(std::size_t offset) noexcept {
            return reinterpret_cast<T*>(bytes + offset * sizeof(T));
        }
        const T* address(std::size_t offset) const noexcept {
            return reinterpret_cast<const T*>(bytes + offset * sizeof(T));
        }
    };

public:
    using value_type = T;
    using size_type  = std::size_t;

    SegmentedDeque() : map_(kInitialMapSlots, nullptr), first_slot_(map_.size() / 2) {}

    ~SegmentedDeque() { clear(); }

    SegmentedDeque(const SegmentedDeque&)            = delete;
    SegmentedDeque& operator=(const SegmentedDeque&) = delete;
    SegmentedDeque(SegmentedDeque&&)                 = delete;
    SegmentedDeque& operator=(SegmentedDeque&&)      = delete;

    // ===================== TODO(C8-1) map 扩张与居中 =================
    // 当首块前或尾块后没有空指针槽时，把 map_ 扩为原来的两倍（至少 8）：
    //   1) 先创建 new_map(new_capacity, nullptr)，此时若分配失败，*this 不变；
    //   2) 计算当前 active_blocks_()；把这些 Block* 从 first_slot_ 起复制到
    //      new_first=(new_capacity-active)/2；
    //   3) map_.swap(new_map); first_slot_=new_first。
    // 只搬指针、绝不搬 Block/T，所以元素地址和引用保持有效。
    // ================================================================

    // ===================== TODO(C8-2) emplace_back ==================
    // end_global = first_offset_ + size_；由除法/取模得到目标 slot/offset。
    // 若目标 slot 超出 map，先 grow_map_ 后重新计算；若该槽为空，new Block。
    // 最后 std::construct_at(block->address(offset), forward<Args>(args)...)。
    // 只有构造成功才 ++size_。若为这次插入新建 Block 而构造抛异常，要 delete
    // 该空块并把槽复位，使元素序列与 size 保持不变。注意：若此前 grow_map_
    // 已成功，map_capacity() 可以增大；这是可观测的容量变化，不要宣称整个对象
    // 状态具有强异常保证。
    // ================================================================
    template <class... Args>
    T& emplace_back(Args&&... args) {
        placeholder_.emplace(std::forward<Args>(args)...);
        return *placeholder_; // 安全占位：不接入容器；测试不会使用此引用。
    }

    // ===================== TODO(C8-3) emplace_front =================
    // 空容器可复用 emplace_back。非空时：
    //   - first_offset_>0：目标仍在首块的 first_offset_-1；
    //   - first_offset_==0：需要 first_slot_ 前一槽（不足先 grow_map_），新建 Block，
    //     目标偏移是 BlockSize-1。
    // 先成功构造，再提交 first_slot_/first_offset_/size_ 的状态变化；异常时回滚新块。
    // ================================================================
    template <class... Args>
    T& emplace_front(Args&&... args) {
        placeholder_.emplace(std::forward<Args>(args)...);
        return *placeholder_;
    }

    void push_back(const T& value) { (void)emplace_back(value); }
    void push_back(T&& value)      { (void)emplace_back(std::move(value)); }
    void push_front(const T& value){ (void)emplace_front(value); }
    void push_front(T&& value)     { (void)emplace_front(std::move(value)); }

    // ===================== TODO(C8-4) pop_back / pop_front ===========
    // 空容器返回 false。否则先 std::destroy_at 对应元素，再减少 size。
    // 若边缘 Block 已没有活元素，delete 并把 map 槽置空；最后一个元素删除后调用
    // reset_empty_ 把首位置复位到 map 中央。不要因为释放块而移动其他块里的 T。
    // ================================================================
    bool pop_back() noexcept { return false; }
    bool pop_front() noexcept { return false; }

    // clear 已给出：只析构真正存在的 size_ 个 T，再释放每个 Block。
    void clear() noexcept {
        for (size_type i = 0; i < size_; ++i) std::destroy_at(element_ptr_(i));
        for (Block*& block : map_) {
            delete block;
            block = nullptr;
        }
        reset_empty_();
    }

    T& operator[](size_type index) noexcept { return *element_ptr_(index); }
    const T& operator[](size_type index) const noexcept { return *element_ptr_(index); }

    T& at(size_type index) {
        if (index >= size_) throw std::out_of_range("SegmentedDeque::at: index out of range");
        return (*this)[index];
    }
    const T& at(size_type index) const {
        if (index >= size_) throw std::out_of_range("SegmentedDeque::at: index out of range");
        return (*this)[index];
    }

    T& front() noexcept { return (*this)[0]; }
    const T& front() const noexcept { return (*this)[0]; }
    T& back() noexcept { return (*this)[size_ - 1]; }
    const T& back() const noexcept { return (*this)[size_ - 1]; }

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    // 观测接口：课程测试用它确认跨块与 map 扩张，而不暴露内部 Block 类型。
    size_type allocated_blocks() const noexcept {
        size_type count = 0;
        for (const Block* block : map_) if (block) ++count;
        return count;
    }
    size_type map_capacity() const noexcept { return map_.size(); }
    static constexpr size_type block_size() noexcept { return BlockSize; }

private:
    static constexpr size_type kInitialMapSlots = 8;

    // TODO(C8-1)：按上面的事务式步骤实现。
    void grow_map_() {
        // 安全占位：不修改 map。
    }

    size_type active_blocks_() const noexcept {
        if (size_ == 0) return 0;
        return (first_offset_ + size_ - 1) / BlockSize + 1;
    }

    T* element_ptr_(size_type index) noexcept {
        size_type global = first_offset_ + index;
        size_type slot   = first_slot_ + global / BlockSize;
        size_type offset = global % BlockSize;
        return map_[slot]->address(offset);
    }
    const T* element_ptr_(size_type index) const noexcept {
        size_type global = first_offset_ + index;
        size_type slot   = first_slot_ + global / BlockSize;
        size_type offset = global % BlockSize;
        return map_[slot]->address(offset);
    }

    void reset_empty_() noexcept {
        size_         = 0;
        first_offset_ = 0;
        first_slot_   = map_.size() / 2;
    }

    std::vector<Block*> map_;
    size_type           first_slot_   = 0;
    size_type           first_offset_ = 0;
    size_type           size_         = 0;

    // 仅供未完成骨架安全返回 T&；optional 不会额外要求 T 可默认构造。
    // 完成 emplace 后不再使用它，也不要把它计入 size_。
    std::optional<T>    placeholder_;
};

} // namespace cppbc
