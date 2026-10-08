// =============================================================================
//  E5 · 数据导向设计（AoS / SoA / cache locality）—— 新手导读
// =============================================================================
//
//  面向对象先问“实体是什么类”，数据导向设计先问“热循环每次真正读取哪些字段”。
//  AoS（Array of Structures）把一个实体的全部字段放一起；SoA（Structure of Arrays）
//  把同一字段排成连续列。两者没有绝对优劣：一次处理完整实体时 AoS 很自然；只扫描
//  少数热字段时 SoA 通常有更小工作集、更适合预取和 SIMD。
//
//  单元测试不能用“必须快 2 倍”这种耗时断言。本模块改用三种确定性证据：
//    - 地址步长：AoS 的相邻 x 相隔 sizeof(Particle)，SoA 相隔 sizeof(float)；
//    - 模型化读取字节：明确算法所需字段和布局跨度，不声称等同硬件 cache miss；
//    - buffer 分配事件：reserve 后由几个独立连续缓冲区承担存储。
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cppbc::dod {

struct Particle {
    float x{};
    float y{};
    float z{};
    float vx{};
    float vy{};
    float vz{};
    std::uint32_t flags{};
};

struct WorkStats {
    std::size_t visited{};
    std::size_t scalar_reads{};
    std::size_t scalar_writes{};
    std::size_t modeled_bytes{};
};

class AoSParticles {
    std::vector<Particle> particles_;
    std::size_t allocation_events_ = 0;

public:
    void reserve(std::size_t count) {
        // TODO(E5-1)：仅当 count > capacity 时 reserve，并把 allocation_events_ 加 1。
        (void)count;
    }

    void push_back(const Particle& particle) {
        // TODO(E5-2)：追加完整记录；若未预留而 vector 即将增长，也记录一次 buffer 增长。
        // 提示：push 前 size()==capacity() 表示这次需要增长。
        (void)particle;
    }

    std::size_t size() const noexcept { return particles_.size(); }
    std::size_t capacity() const noexcept { return particles_.capacity(); }
    std::size_t allocation_events() const noexcept { return allocation_events_; }

    Particle& operator[](std::size_t index) noexcept { return particles_[index]; }
    const Particle& operator[](std::size_t index) const noexcept { return particles_[index]; }
    const Particle* data() const noexcept { return particles_.data(); }
};

class SoAParticles {
    std::vector<float> xs_;
    std::vector<float> ys_;
    std::vector<float> zs_;
    std::vector<float> vxs_;
    std::vector<float> vys_;
    std::vector<float> vzs_;
    std::vector<std::uint32_t> flags_;
    std::size_t allocation_events_ = 0;

public:
    static constexpr std::size_t kColumnCount = 7;

    void reserve(std::size_t count) {
        // TODO(E5-3)：分别检查七个列 vector；仅当 count 大于该列 capacity 时
        // reserve(count)，并为每个实际增长的 buffer 把 allocation_events_ 加 1。
        (void)count;
    }

    void push_back(const Particle& particle) {
        // TODO(E5-4)：把每个字段追加到对应列。每列 push 前分别检查
        // size()==capacity()，若该列将增长就把 allocation_events_ 加 1。
        // 不要假设不同 vector 实例或元素类型具有相同的实际 capacity/增长策略。
        (void)particle;
    }

    std::size_t size() const noexcept { return xs_.size(); }
    // 能保证“不让任何一列重分配”而继续追加的完整实体容量，取七列最小值。
    std::size_t capacity() const noexcept {
        return std::min({xs_.capacity(), ys_.capacity(), zs_.capacity(),
                         vxs_.capacity(), vys_.capacity(), vzs_.capacity(),
                         flags_.capacity()});
    }
    std::size_t allocation_events() const noexcept { return allocation_events_; }

    std::array<std::size_t, kColumnCount> column_capacities() const noexcept {
        return {xs_.capacity(), ys_.capacity(), zs_.capacity(),
                vxs_.capacity(), vys_.capacity(), vzs_.capacity(),
                flags_.capacity()};
    }

    float& x(std::size_t i) noexcept { return xs_[i]; }
    float& y(std::size_t i) noexcept { return ys_[i]; }
    float& z(std::size_t i) noexcept { return zs_[i]; }
    float& vx(std::size_t i) noexcept { return vxs_[i]; }
    float& vy(std::size_t i) noexcept { return vys_[i]; }
    float& vz(std::size_t i) noexcept { return vzs_[i]; }

    float x(std::size_t i) const noexcept { return xs_[i]; }
    float y(std::size_t i) const noexcept { return ys_[i]; }
    float z(std::size_t i) const noexcept { return zs_[i]; }
    float vx(std::size_t i) const noexcept { return vxs_[i]; }
    float vy(std::size_t i) const noexcept { return vys_[i]; }
    float vz(std::size_t i) const noexcept { return vzs_[i]; }
    std::uint32_t flags(std::size_t i) const noexcept { return flags_[i]; }

    const float* x_data() const noexcept { return xs_.data(); }
};

inline void advance(AoSParticles& particles, float dt, WorkStats& stats) noexcept {
    // TODO(E5-5)：逐实体做 x+=vx*dt、y+=vy*dt、z+=vz*dt。
    // 每个实体 visited+1、scalar_reads+6（位置和速度）、scalar_writes+3，
    // modeled_bytes += sizeof(Particle)（AoS 热循环跨过整条记录的确定性模型）。
    (void)particles;
    (void)dt;
    (void)stats;
}

inline void advance(SoAParticles& particles, float dt, WorkStats& stats) noexcept {
    // TODO(E5-6)：同样更新三列位置并记录 visited/read/write；模型字节为每实体
    // 6*sizeof(float)，因为本循环只遍历六个热列，不读 flags 列。
    (void)particles;
    (void)dt;
    (void)stats;
}

inline float sum_x(const AoSParticles& particles, WorkStats& stats) noexcept {
    // TODO(E5-7)：累加 x；每实体 visited/read 各 +1，模型字节 += sizeof(Particle)。
    (void)particles;
    (void)stats;
    return 0.0F;
}

inline float sum_x(const SoAParticles& particles, WorkStats& stats) noexcept {
    // TODO(E5-8)：累加连续 x 列；每实体 visited/read 各 +1，模型字节 += sizeof(float)。
    (void)particles;
    (void)stats;
    return 0.0F;
}

} // namespace cppbc::dod
