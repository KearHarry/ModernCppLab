// =============================================================================
//  A8 · tagged union：简化 Variant<int, std::string> 与 visit
// -----------------------------------------------------------------------------
//  骨架保留“值构造、移动赋值、visit”三处安全占位；reset、复制/移动构造、复制赋值、
//  emplace 与受检访问已给出，因此 emplace 可以安全激活成员。未完成时测试会红，但不会
//  读取或析构未构造的 union 成员。
// =============================================================================
#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace cppbc {

struct BadVariantAccess : std::exception {
    const char* what() const noexcept override { return "bad variant access"; }
};

class IntStringVariant {
public:
    enum class Kind { Empty, Int, String };

    IntStringVariant() noexcept = default;

    // ===================== TODO(A8-1) 值构造 ========================
    // int 是隐式生命周期类型，可直接赋值；string 必须 placement new。
    // 构造成功后再把 kind_ 提交为对应标签。
    // =================================================================
    IntStringVariant(int value) noexcept { (void)value; }
    IntStringVariant(std::string value) { (void)value; }
    IntStringVariant(const char* value)
        : IntStringVariant(value ? std::string(value)
                                 : throw std::invalid_argument("variant string is null")) {}

    // ===================== 已给出(A8-2) reset / 析构 =================
    // 仅当当前标签是 String 时显式调用析构；int 无需析构；最后置 Empty。
    // =================================================================
    void reset() noexcept {
        if (kind_ == Kind::String) {
            std::destroy_at(&storage_.s);
        }
        kind_ = Kind::Empty;
    }
    ~IntStringVariant() { reset(); }

    // ===================== 已给出(A8-3) 拷贝构造 =====================
    // 根据 other.kind() 拷贝活跃成员；string 用 placement new。
    // =================================================================
    IntStringVariant(const IntStringVariant& other) {
        if (other.kind_ == Kind::String) {
            ::new (static_cast<void*>(&storage_.s)) std::string(other.storage_.s);
        } else if (other.kind_ == Kind::Int) {
            storage_.i = other.storage_.i;
        }
        kind_ = other.kind_;
    }

    // ===================== 已给出(A8-4) 移动构造 =====================
    // 移动构造活跃成员，成功后 reset other；移动后源统一为空是本类约定。
    // =================================================================
    IntStringVariant(IntStringVariant&& other) noexcept {
        if (other.kind_ == Kind::String) {
            ::new (static_cast<void*>(&storage_.s))
                std::string(std::move(other.storage_.s));
        } else if (other.kind_ == Kind::Int) {
            storage_.i = other.storage_.i;
        }
        kind_ = other.kind_;
        other.reset();
    }

    // ===================== 已给出(A8-5a) 拷贝赋值 ====================
    // 正确处理同类型赋值和 int/string 切换，并保持自赋值安全。
    // =================================================================
    IntStringVariant& operator=(const IntStringVariant& other) {
        if (this == &other) return *this;
        if (other.kind_ == Kind::String) emplace_string(other.storage_.s);
        else if (other.kind_ == Kind::Int) emplace_int(other.storage_.i);
        else reset();
        return *this;
    }

    // ===================== TODO(A8-5b) 移动赋值 ======================
    // 根据 other 的活动成员进行移动；成功后 reset other，使源统一为空。
    // 要处理自移动。当前占位保持两端原状态不变，因此安全但测试为红。
    // =================================================================
    IntStringVariant& operator=(IntStringVariant&& other) noexcept {
        (void)other;
        return *this;
    }

    // ===================== 已给出(A8-6) emplace ======================
    // reset 旧成员后构造新成员；string 构造可能抛异常，此时保持 Empty。
    // =================================================================
    void emplace_int(int value) noexcept {
        if (kind_ == Kind::String) {
            std::destroy_at(&storage_.s);
        }
        storage_.i = value;
        kind_ = Kind::Int;
    }
    void emplace_string(std::string value) {
        if (kind_ == Kind::String) {
            storage_.s = std::move(value);
            return;
        }
        kind_ = Kind::Empty;
        ::new (static_cast<void*>(&storage_.s)) std::string(std::move(value));
        kind_ = Kind::String;
    }
    void emplace_string(const char* value) {
        if (value == nullptr) {
            throw std::invalid_argument("variant string is null");
        }
        emplace_string(std::string(value));
    }

    Kind kind() const noexcept { return kind_; }
    bool empty() const noexcept { return kind_ == Kind::Empty; }
    bool holds_int() const noexcept { return kind_ == Kind::Int; }
    bool holds_string() const noexcept { return kind_ == Kind::String; }

    // ===================== 已给出(A8-7) get_if / get =================
    int* get_if_int() noexcept {
        return holds_int() ? &storage_.i : nullptr;
    }
    const int* get_if_int() const noexcept {
        return holds_int() ? &storage_.i : nullptr;
    }
    std::string* get_if_string() noexcept {
        return holds_string() ? &storage_.s : nullptr;
    }
    const std::string* get_if_string() const noexcept {
        return holds_string() ? &storage_.s : nullptr;
    }

    int& get_int() {
        if (auto* p = get_if_int()) return *p;
        throw BadVariantAccess{};
    }
    const int& get_int() const {
        if (auto* p = get_if_int()) return *p;
        throw BadVariantAccess{};
    }
    std::string& get_string() {
        if (auto* p = get_if_string()) return *p;
        throw BadVariantAccess{};
    }
    const std::string& get_string() const {
        if (auto* p = get_if_string()) return *p;
        throw BadVariantAccess{};
    }

    // ===================== TODO(A8-8) visit ==========================
    // 根据标签调用 visitor(int&) 或 visitor(string&)；Empty 抛异常。
    // 两个分支必须具有相同返回类型。const 重载传 const 引用。
    // =================================================================
    template <class Visitor>
    std::invoke_result_t<Visitor, int&> visit(Visitor&& visitor) {
        using IntResult = std::invoke_result_t<Visitor, int&>;
        using StringResult = std::invoke_result_t<Visitor, std::string&>;
        static_assert(std::is_same_v<IntResult, StringResult>,
                      "visitor 的 int/string 分支必须返回相同类型");
        (void)visitor;
        throw BadVariantAccess{}; // 安全占位
    }

    template <class Visitor>
    std::invoke_result_t<Visitor, const int&> visit(Visitor&& visitor) const {
        using IntResult = std::invoke_result_t<Visitor, const int&>;
        using StringResult = std::invoke_result_t<Visitor, const std::string&>;
        static_assert(std::is_same_v<IntResult, StringResult>,
                      "visitor 的 int/string 分支必须返回相同类型");
        (void)visitor;
        throw BadVariantAccess{}; // 安全占位
    }

private:
    union Storage {
        int i;
        std::string s;
        Storage() noexcept {}
        ~Storage() noexcept {}
    } storage_;

    Kind kind_ = Kind::Empty;
};

} // namespace cppbc
