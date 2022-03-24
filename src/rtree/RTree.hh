// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <memory>

template<
    typename T, typename Domain, typename Allocator = std::allocator<T>,
    std::size_t LeafSize = 64, std::size_t InnerSize = 64,
    typename NodeSizeT = uint32_t,
    std::size_t LeafMin = LeafSize / 3, std::size_t LeafReinsert = LeafSize / 3,
    std::size_t InnerMin = InnerSize / 3, std::size_t InnerReinsert = InnerSize / 3
> class RStar;

template<
    typename T, typename Domain, typename Allocator = std::allocator<T>,
    std::size_t LeafSize = 64, std::size_t InnerSize = 64,
    typename NodeSizeT = uint32_t
> class RTree {
public:
    template<
        typename T_, typename Domain_, typename Allocator_,
        std::size_t LeafSize_, std::size_t InnerSize_,
        typename NodeSizeT_,
        std::size_t LeafMin, std::size_t LeafReinsert,
        std::size_t InnerMin, std::size_t InnerReinsert
    > friend class RStar;
    using RStarInserter = RStar<T, Domain, Allocator, LeafSize, InnerSize, NodeSizeT>;
    using domain = Domain;
    using level_t = int;

protected:
    using size_t = std::size_t;
    struct Node {
        NodeSizeT size = 0;
        typename Domain::rect_t rect;
        union {
            Node *children;
            T *objects;
        };
    };
    using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    level_t mDepth = 0;
    Node mRoot {};
    Domain mDomain;
    Allocator mAllocator;
    NodeAllocator mNodeAllocator;

    void boundInner(Node &node) const {
        node.rect = node.children[0].rect;
        for (size_t i = 1; i < node.size; ++i) {
            node.rect = mDomain.union_(node.rect, node.children[i].rect);
        }
    }
    void boundLeaf(Node &node) const {
        node.rect = mDomain.rect(node.objects[0]);
        for (size_t i = 1; i < node.size; ++i) {
            node.rect = mDomain.union_(node.rect, mDomain.rect(node.objects[i]));
        }
    }

    T *allocateLeafArray() {
        return std::allocator_traits<Allocator>::allocate(mAllocator, LeafSize);
    }
    Node *allocateNodeArray() {
        return std::allocator_traits<NodeAllocator>::allocate(mNodeAllocator, InnerSize);
    }
    void destroySubtree(Node &node, level_t level) {
        if (level > 0) {
            for (size_t i = 0; i < node.size; ++i) {
                destroySubtree(node.children[i], level - 1);
                node.children[i].~Node();
            }
            std::allocator_traits<NodeAllocator>::deallocate(mNodeAllocator, node.children, InnerSize);
        } else {
            for (size_t i = 0; i < node.size; ++i) {
                node.objects[i].~T();
            }
            std::allocator_traits<Allocator>::deallocate(mAllocator, node.objects, LeafSize);
        }
    }

    template<typename Visitor>
    bool visitNode(Visitor &visitor, Node &node, level_t level) {
        if (node.size == 0 || !visitor.check(node.rect, level)) {return true;}
        if (level > 0) {
            for (size_t i = 0; i < node.size; ++i) {
                if (!visitNode(visitor, node.children[i], level - 1)) {return false;}
            }
        } else {
            for (size_t i = 0; i < node.size; ++i) {
                if (!visitor.visit(node.objects[i])) {return false;}
            }
        }
        return true;
    }
    template<typename Visitor>
    bool visitNode(Visitor &visitor, const Node &node, level_t level) const {
        if (node.size == 0 || !visitor.check(node.rect, level)) {return true;}
        if (level > 0) {
            for (size_t i = 0; i < node.size; ++i) {
                if (!visitNode(visitor, node.children[i], level - 1)) {return false;}
            }
        } else {
            for (size_t i = 0; i < node.size; ++i) {
                if (!visitor.visit(node.objects[i])) {return false;}
            }
        }
        return true;
    }

public:
    RTree(const Domain &domain = {}, const Allocator &alloc = {}) : mDomain{domain}, mAllocator{alloc}, mNodeAllocator{alloc} {}

    // TODO: implement copying
    RTree(const RTree &) = delete;
    void operator=(const RTree &other) = delete;

    RTree(RTree &&other) {
        *this = std::forward<RTree>(other);
    }
    void operator=(RTree &&other) {
        mDepth = other.mDepth;
        mRoot = other.mRoot;
        other.mDepth = 0;
        other.mRoot.size = 0;
        other.mRoot.objects = nullptr;
    }

    ~RTree() {clear();}
    void clear() {
        if (mRoot.size) {destroySubtree(mRoot, mDepth);}
        mRoot.size = 0;
        mDepth = 0;
    }

    template<typename Check, typename Visit>
    struct visitor {
        Check checkFn;
        Visit visitFn;

        visitor(Check &&check, Visit &&visit) : checkFn{check}, visitFn{visit} {}
        bool check(const typename Domain::rect_t &a, int level) {return checkFn(a, level);}
        bool visit(const T &a) {return visitFn(a);}
    };

    template<typename Visitor>
    void visit(Visitor &&visitor) {
        visitNode(visitor, mRoot, mDepth);
    }
    template<typename Check, typename Visit>
    void visit(Check &&check, Visit &&visit) {
        this->visit(visitor<Check, Visit>(std::forward<Check>(check), std::forward<Visit>(visit)));
    }
    template<typename Visitor>
    void visit(Visitor &&visitor) const {
        visitNode(visitor, mRoot, mDepth);
    }
    template<typename Check, typename Visit>
    void visit(Check &&check, Visit &&visit) const {
        this->visit(visitor<Check, Visit>(std::forward<Check>(check), std::forward<Visit>(visit)));
    }
    level_t depth() const {return mDepth;}
};
