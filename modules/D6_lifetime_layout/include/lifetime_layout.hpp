// =============================================================================
//  D6 · 对象生命周期与内存布局 —— 新手导读
// =============================================================================
//
//  本模块把几个看似零散、实际共享同一条主线的面试题放进可观测实验：
//    1) 基类、成员、构造函数体与析构函数体的先后顺序；
//    2) 临时对象绑定到局部 const 引用后的生命周期延长；
//    3) padding / alignment，以及按声明顺序重排成员为什么可能缩小对象；
//    4) 空基类优化(EBO)与 C++20 [[no_unique_address]]。
//
//  所有实验都只观察标准保证的行为，不读取 padding 字节、不猜 vptr 位置，也不制造悬垂
//  引用。请实现标有 TODO(D6-x) 的位置，让测试从红变绿。
// =============================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppbc::lifetime_layout {

// 简单事件记录器：构造/析构实验通过文本序列观察顺序。
class EventLog {
    std::vector<std::string> events_;

public:
    void record(std::string event) { events_.push_back(std::move(event)); }
    const std::vector<std::string>& events() const noexcept { return events_; }
};

class BaseLayer {
    EventLog* log_;
    std::string label_;

public:
    BaseLayer(EventLog& log, std::string label)
        : log_(&log), label_(std::move(label)) {
        log_->record(label_ + " ctor");
    }
    ~BaseLayer() { log_->record(label_ + " dtor"); }
};

class MemberLayer {
    EventLog* log_;
    std::string label_;

public:
    MemberLayer(EventLog& log, std::string label)
        : log_(&log), label_(std::move(label)) {
        log_->record(label_ + " ctor");
    }
    ~MemberLayer() { log_->record(label_ + " dtor"); }
};

// 注意：真正决定成员初始化顺序的是“声明顺序”，不是初始化列表的书写顺序。
// 工程代码仍应让初始化列表保持同序，以免误导读者和触发 -Wreorder；事件测试观察的是
// 语言实际规定的 base -> first_ -> second_ -> constructor body 顺序。
class LifetimeObject : private BaseLayer {
    MemberLayer first_;
    MemberLayer second_;
    EventLog* log_;

public:
    explicit LifetimeObject(EventLog& log)
        : BaseLayer(log, "<TODO-base>"),
          first_(log, "<TODO-first>"),
          second_(log, "<TODO-second>"),
          log_(&log) {
        // TODO(D6-1)：把三处占位标签改为 base/first/second，并在这里记录
        //               "derived body"。析构函数体记录 "derived dtor"。
        log_->record("<TODO-derived-body>");
    }

    ~LifetimeObject() {
        log_->record("<TODO-derived-dtor>");
    }
};

// 用计数器观察对象是否仍然存活；不暴露任何可能悬垂的引用。
class LiveToken {
    int* live_;

public:
    explicit LiveToken(int& live) noexcept : live_(&live) { ++*live_; }
    LiveToken(const LiveToken&) = delete;
    LiveToken& operator=(const LiveToken&) = delete;
    ~LiveToken() { --*live_; }
};

inline bool temporary_survives_local_const_reference() {
    // TODO(D6-2)：先令 int live=0、bool alive_inside=false；在内层作用域写：
    //   const LiveToken& token = LiveToken(live);
    //   (void)token;
    //   alive_inside = (live == 1);
    // 离开作用域后返回 alive_inside && live==0。不要返回 token 的引用。
    return false;
}

// alignment 必须是非零的 2 的幂、其值能由 uintptr_t 表示，并且向上取整后的结果
// 也必须可表示，即 value <= UINTPTR_MAX - (alignment - 1)。本实验把前置条件写进接口注释；
// 生产接口宜返回 optional/错误码来显式报告非法对齐或溢出。
constexpr std::uintptr_t align_up(std::uintptr_t value,
                                  std::size_t alignment) noexcept {
    // TODO(D6-3)：使用 (value + alignment - 1) & ~(alignment - 1)。
    (void)alignment;
    return value;
}

template <class T>
bool is_aligned_for(const void* p) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(p);
    return address % alignof(T) == 0;
}

// 相同有效载荷，不同成员顺序。对象大小与具体数值由 ABI 决定；测试不要求编译器一定
// 实施成员重排收益、EBO 或 [[no_unique_address]] 优化，只由 LayoutReport 暴露真实结果。
struct PoorLayout {
    char first;
    double value;
    char last;
};

struct CompactLayout {
    double value;
    char first;
    char last;
};

struct EmptyPolicy {};

struct PlainPolicyHolder {
    EmptyPolicy policy;
    int value;
};

struct EboPolicyHolder : private EmptyPolicy {
    int value;
};

struct AttributePolicyHolder {
    [[no_unique_address]] EmptyPolicy policy;
    int value;
};

struct LayoutReport {
    std::size_t poor_size{};
    std::size_t compact_size{};
    std::size_t poor_alignment{};
    std::size_t compact_alignment{};
    std::size_t plain_policy_size{};
    std::size_t ebo_policy_size{};
    std::size_t attribute_policy_size{};
};

inline LayoutReport inspect_layout() noexcept {
    // TODO(D6-4)：用 sizeof/alignof 填满报告。不要把任何平台的数字写死。
    return {};
}

static_assert(std::is_standard_layout_v<PoorLayout>);
static_assert(std::is_standard_layout_v<CompactLayout>);
static_assert(sizeof(PoorLayout) >= sizeof(char) * 2 + sizeof(double));
static_assert(sizeof(CompactLayout) >= sizeof(char) * 2 + sizeof(double));

} // namespace cppbc::lifetime_layout
