// =============================================================================
//  C7 · 简化 ordered_map：红黑树 —— TODO 骨架
// =============================================================================
//
//  每个节点保存 key/value、颜色、左右孩子与父指针。nullptr 叶子按黑色处理。
//  学员实现查找、旋转、插入修复、删除修复；只读的不变量检查器已给出，可在每一步后
//  自动验证：BST 有序、根黑、红节点孩子黑、每条根到空叶路径黑高相等、父指针正确。
//
//  骨架的修改操作不创建节点，查询返回空。因此开箱可安全运行并确定性红测。
// =============================================================================
#pragma once

#include <cstddef>
#include <functional>
#include <utility>

namespace cppbc {

template <class K, class V, class Compare = std::less<K>>
class RedBlackTree {
    enum class Color { Red, Black };

    struct Node {
        K     key;
        V     value;
        Color color  = Color::Red;
        Node* parent = nullptr;
        Node* left   = nullptr;
        Node* right  = nullptr;

        template <class KK, class VV>
        Node(KK&& k, VV&& v)
            : key(std::forward<KK>(k)), value(std::forward<VV>(v)) {}
    };

public:
    struct Validation {
        bool        ok           = true;
        std::size_t node_count   = 0;
        int         black_height = 1; // nullptr 黑叶本身计一层。
    };

    RedBlackTree() = default;
    ~RedBlackTree() { destroy_(root_); }

    RedBlackTree(const RedBlackTree&)            = delete;
    RedBlackTree& operator=(const RedBlackTree&) = delete;

    // ===================== TODO(C7-1) find / lower_bound ============
    // find_node_：按 Compare 三分：key<node 向左，node<key 向右，否则相等。
    // lower_bound_node_：一路搜索“首个不小于 key”的候选；当前 key 不小于目标时
    // 记下候选并向左，否则向右。两者均为 O(height)。
    // ===============================================================
    V* find(const K& key) {
        (void)key;
        return nullptr;
    }
    const V* find(const K& key) const {
        (void)key;
        return nullptr;
    }
    const K* lower_bound_key(const K& key) const {
        (void)key;
        return nullptr;
    }

    bool contains(const K& key) const { return find(key) != nullptr; }

    // ===================== TODO(C7-2) 左旋 / 右旋 ===================
    // left_rotate_(x)：令 y=x->right，把 y 的左子树移给 x 的右边，再让 y 取代 x，
    // 最后把 x 接成 y->left。每次接线都要同步更新 parent，且 x 可能是 root_。
    // right_rotate_ 完全镜像。旋转只改链接，不改变中序顺序与节点地址。
    // ===============================================================

    // ===================== TODO(C7-3) 插入或更新 ===================
    // 1) 像普通 BST 一样找位置；等价 key 已存在则只赋 value，返回 false。
    // 2) new 一个红节点，挂到找到的 parent 下，++size_。
    // 3) insert_fix_(z)：当 parent 是红色，根据 uncle 的颜色处理：
    //      uncle 红：parent/uncle 涂黑、grandparent 涂红，z 上移；
    //      uncle 黑：先把“折线”旋成直线，再旋 grandparent 并交换颜色。
    //    左右情形镜像；循环后强制 root_ 黑色。
    // 4) 构造/比较/赋值异常前不得破坏现有结构；节点所有权接入后再改 size。
    // ===============================================================
    template <class KK, class VV>
    bool insert_or_assign(KK&& key, VV&& value) {
        (void)key;
        (void)value;
        return false; // 安全占位：树保持空。
    }

    // ===================== TODO(C7-4) 删除与双黑修复 =================
    // 先按普通 BST 删除：
    //   - 至多一个孩子：用 transplant_ 直接替换；
    //   - 两个孩子：取右子树最小后继 y，把 y 移到 z 的位置并继承 z 的颜色。
    // 若真正从树中移走的颜色是黑色，黑高少一，需要 erase_fix_(x, parent)：
    //   兄弟红 -> 兄弟变黑、父变红并旋转，转为兄弟黑；
    //   兄弟两孩黑 -> 兄弟变红，双黑上移；
    //   远侄黑/近侄红 -> 先旋兄弟；
    //   远侄红 -> 兄弟继承父色，父和远侄变黑，旋父并结束。
    // nullptr 也可能携带“双黑”，所以必须额外传 parent，不能解引用 x。
    // 成功 delete 并 --size_，未找到返回 false。
    // ===============================================================
    bool erase(const K& key) {
        (void)key;
        return false;
    }

    std::size_t size()  const noexcept { return size_; }
    bool        empty() const noexcept { return size_ == 0; }

    template <class F>
    void for_each(F&& f) const {
        inorder_(root_, f);
    }

    // 已给出的“不变量裁判”。它不依赖某个固定树形，只检查红黑树定义。
    Validation validate() const {
        Validation result;
        if (root_ && root_->parent != nullptr) result.ok = false;
        if (root_ && root_->color != Color::Black) result.ok = false;

        Check c = validate_node_(root_, nullptr, nullptr, nullptr);
        result.ok = result.ok && c.ok && c.count == size_;
        result.node_count = c.count;
        result.black_height = c.black_height;
        return result;
    }

private:
    struct Check {
        bool        ok;
        std::size_t count;
        int         black_height;
    };

    static bool is_black_(const Node* n) noexcept {
        return n == nullptr || n->color == Color::Black;
    }

    void left_rotate_(Node* x) noexcept {
        (void)x; // TODO(C7-2)
    }
    void right_rotate_(Node* x) noexcept {
        (void)x; // TODO(C7-2)
    }
    void insert_fix_(Node* z) noexcept {
        (void)z; // TODO(C7-3)
    }
    void erase_fix_(Node* x, Node* parent) noexcept {
        (void)x;
        (void)parent; // TODO(C7-4)
    }

    Node* find_node_(const K& key) const {
        (void)key;
        return nullptr; // TODO(C7-1)
    }

    static Node* minimum_(Node* n) noexcept {
        while (n && n->left) n = n->left;
        return n;
    }

    void transplant_(Node* old_node, Node* replacement) noexcept {
        if (!old_node->parent) root_ = replacement;
        else if (old_node == old_node->parent->left) old_node->parent->left = replacement;
        else old_node->parent->right = replacement;
        if (replacement) replacement->parent = old_node->parent;
    }

    template <class F>
    static void inorder_(const Node* n, F& f) {
        if (!n) return;
        inorder_(n->left, f);
        f(n->key, n->value);
        inorder_(n->right, f);
    }

    void destroy_(Node* n) noexcept {
        if (!n) return;
        destroy_(n->left);
        destroy_(n->right);
        delete n;
    }

    Check validate_node_(const Node* n, const Node* expected_parent,
                         const K* lower, const K* upper) const {
        if (!n) return {true, 0, 1};

        bool ok = n->parent == expected_parent;
        if (lower && !comp_(*lower, n->key)) ok = false;
        if (upper && !comp_(n->key, *upper)) ok = false;
        if (n->color == Color::Red && (!is_black_(n->left) || !is_black_(n->right)))
            ok = false;

        Check left  = validate_node_(n->left, n, lower, &n->key);
        Check right = validate_node_(n->right, n, &n->key, upper);
        if (left.black_height != right.black_height) ok = false;

        return {
            ok && left.ok && right.ok,
            1 + left.count + right.count,
            left.black_height + (n->color == Color::Black ? 1 : 0)
        };
    }

    Node*       root_ = nullptr;
    std::size_t size_ = 0;
    Compare     comp_{};
};

} // namespace cppbc
