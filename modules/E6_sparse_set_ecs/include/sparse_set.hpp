// =============================================================================
//  E6 · Sparse Set / ECS 组件存储 —— 可编译课程骨架
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace cppbc {

using Entity = std::uint32_t;
inline constexpr std::size_t kSparseNpos =
    std::numeric_limits<std::size_t>::max();

template <class T>
class SparseSet {
public:
    bool contains(Entity entity) const noexcept {
        if (entity >= sparse_.size()) return false;
        const auto dense_index = sparse_[entity];
        return dense_index < entities_.size() && entities_[dense_index] == entity;
    }

    T* get(Entity entity) noexcept {
        return contains(entity) ? &components_[sparse_[entity]] : nullptr;
    }
    const T* get(Entity entity) const noexcept {
        return contains(entity) ? &components_[sparse_[entity]] : nullptr;
    }

    // TODO(E6-1)：若 entity 已存在就更新组件；否则扩 sparse_，并把实体和组件
    // 同步 push 到两个 dense 数组，再记录 entity -> dense index。
    template <class U>
    T& insert(Entity entity, U&& component) {
        (void)entity;
        placeholder_.emplace(std::forward<U>(component));
        return *placeholder_; // 骨架：不插入，测试先红且不会越界
    }

    // TODO(E6-2)：swap-and-pop 删除。把最后一个 dense 元素搬到洞里，并修正
    // 被搬实体的 sparse 索引；最后弹出两个 dense 数组并把旧 sparse 槽置 npos。
    bool erase(Entity entity) {
        (void)entity;
        return false;
    }

    // TODO(E6-3)：按 dense 顺序调用 fn(entity, component)。这是 ECS 快速顺序遍历的核心。
    template <class Fn>
    void each(Fn&& fn) {
        (void)fn;
    }

    std::size_t size() const noexcept { return entities_.size(); }
    bool empty() const noexcept { return entities_.empty(); }
    const std::vector<Entity>& entities() const noexcept { return entities_; }
    const std::vector<T>& components() const noexcept { return components_; }

private:
    std::vector<std::size_t> sparse_;
    std::vector<Entity> entities_;
    std::vector<T> components_;
    // 仅让未完成的骨架能安全返回引用，不给 SparseSet 强加“T 必须默认构造”的限制。
    std::optional<T> placeholder_;
};

} // namespace cppbc
