// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include "RTree.hh"

template<
    typename T, typename Domain, typename Allocator,
    std::size_t LeafSize, std::size_t InnerSize,
    typename NodeSizeT,
    std::size_t LeafMin, std::size_t LeafReinsert,
    std::size_t InnerMin, std::size_t InnerReinsert
> class RStar {
public:
    using Tree = RTree<T, Domain, Allocator, LeafSize, InnerSize, NodeSizeT>;
    using level_t = typename Tree::level_t;
    static constexpr std::size_t leaf_size = LeafSize, inner_size = InnerSize;
    static_assert(std::numeric_limits<NodeSizeT>::max() > LeafSize);
    static_assert(std::numeric_limits<NodeSizeT>::max() > InnerSize);
    // non-root nodes must not be empty
    static_assert(InnerMin >= 1 && LeafMin >= 1);
    // ensure that there is at least one valid split index
    // also guarantees that nodes can have at least 3 children
    static_assert(InnerMin < InnerSize && InnerMin < InnerSize / 2);
    static_assert(LeafMin < LeafSize && LeafMin < LeafSize / 2);

    static_assert(InnerReinsert <= InnerSize - InnerMin + 1);
    static_assert(LeafReinsert <= LeafSize - LeafMin + 1);

private:
    using Node = typename Tree::Node;
    using NodeAllocator = typename Tree::NodeAllocator;
    using rect_t = typename Domain::rect_t;
    using measure_t = typename Domain::measure_t;
    using margin_t = typename Domain::margin_t;
    using pos_t = typename Domain::pos_t;

    // need to limit the depth to be able to allocate a reinsert mask
    static constexpr level_t max_depth = 63;
    using ReinsertMask = std::bitset<max_depth + 1>;

    Tree &tree;

    // The R* insertion algorithm has 2 layers of recursion:
    // • reinsertion: one reinsertion may lead to another
    // • tree descent
    // To prevent quadratic (in tree depth) stack growth, the tree descent returns
    // completely before reinsertion starts (this is also a natural fit for the
    // operation of tightening the enclosing rectangles of a reinserted subtree).
    // The buffer for reinserted elements is allocated by the reinsertion
    // recursion, in the form of an instance of this class. All levels of recursion
    // share one reinsertion mask (which records if reinsertion was already done
    // at any given level) which is allocated by the top-level static method call
    // and passed by reference to each level of reinsertion.
    //
    // A level of tree descent may return with any of 3 results:
    // • the element was inserted normally: no action needed by the caller.
    //   This is signaled by returning an empty node and leaving
    //   this->reinserts at 0.
    // • reinsertion was initiated: the bounding rectangle of the supertree roots
    //   must be tigthened. This is signaled by returning an empty node and
    //   setting this->reinserts and this->reinsertLevel appropriately.
    // • node was split: caller must either add the new node (return value) as
    //   a new child or start reinsertion/splitting itself. Signaled by returning a
    //   non-empty node.
    ReinsertMask &reinsertMask;
    size_t reinserts = 0;
    level_t reinsertLevel;
    union {
        std::array<T, LeafSize + 1> objects;
        std::array<Node, InnerSize + 1> children;
    };

    RStar(Tree &tree, ReinsertMask &mask) : tree{tree}, reinsertMask{mask} {}

    size_t chooseInner(const Node &node, rect_t insRect) const {
        size_t best = 0;
        const auto &dom = tree.mDomain;
        auto smallestArea = dom.area(node.children[0].rect);
        auto leastEnlargement = dom.area(dom.union_(node.children[0].rect, insRect)) - smallestArea;
        for (size_t i = 1; i < node.size; ++i) {
            auto area = dom.area(node.children[i].rect);
            auto enlargement = dom.area(
                dom.union_(node.children[i].rect, insRect)
            ) - area;
            if (std::tie(enlargement, area) < std::tie(leastEnlargement, smallestArea)) {
                best = i;
                smallestArea = area;
                leastEnlargement = enlargement;
            }
        }
        return best;
    }
    size_t chooseLeaf(const Node &node, rect_t insRect) const {
        return chooseInner(node, insRect);
#if 0 // GCC produces warnings for this dead code
        // this is the last-level subtree choice algorithm as described in the paper.
        // it makes the construction ~10 times as slow on my machine
        // and makes the tree worse (at least for the integer interval test program).
        // not sure I understand it right. anyway, that's why this is disabled
        // by the return above
        static const size_t prefilterSize = 32;
        const auto &dom = tree.mDomain;
        std::array<std::pair<measure_t, NodeSizeT>, InnerSize> prefilter;
        for (size_t i = 0; i < node.size; ++i) {
            auto rect = node.children[i].rect;
            auto newRect = dom.union_(rect, insRect);
            auto area = dom.area(rect), newArea = dom.area(newRect);
            assert(newArea >= area);
            auto areaEnl = newArea - area;
            prefilter[i] = {areaEnl, i};
        }
        std::sort(prefilter.begin(), prefilter.end());

        size_t best = 0;
        measure_t leastOverlapEnl;
        for (size_t i_ = 0; i_ < std::max(size_t(node.size), prefilterSize); ++i_) {
            size_t i = prefilter[i_].second;
            auto rect = node.children[i].rect;
            auto newRect = dom.union_(rect, insRect);
            measure_t overlap(0), newOverlap(0);
            for (size_t j = 0; j < node.size; ++j) {
                if (i == j) {continue;}
                auto rect_ = node.children[j].rect;
                auto ints = dom.intersect(rect, rect_);
                if (ints.has_value()) {overlap += dom.area(*ints);}
                ints = dom.intersect(newRect, rect_);
                if (ints.has_value()) {newOverlap += dom.area(*ints);}
            }
            assert(newOverlap >= overlap);
            auto overlapEnl = newOverlap - overlap;
            auto area = dom.area(rect), newArea = dom.area(newRect);
            assert(newArea >= area);
            auto areaEnl = newArea - area;
            if (i == 0 || overlapEnl < overlapEnl) {
                best = i;
                leastOverlapEnl = overlapEnl;
            }
        }
        return best;
#endif
    }

    // this function is a complete heuristic. it may reorder `buf` and returns
    // the index of the first element that should go into the second node
    // (minSize <= returnval <= Size - MinSize)
    template<typename E, size_t Size, typename GetRect>
    size_t computeSplit(std::array<E, Size> &buf, GetRect &&getRect, size_t minSize) const {
        auto &dom = tree.mDomain;
        typename Domain::margin_t bestS(0);
        size_t bestAxis = 0;
        for (size_t axis = 0; axis < Domain::dimension; ++axis) {
            std::sort(buf.begin(), buf.end(), [&] (const E &a, const E &b) {
                return dom.cmp(axis, getRect(a), getRect(b));
            });

            // decide split axis based on sum of margin values
            typename Domain::margin_t S(0);
            rect_t rect1 = getRect(buf[0]), rect2 = getRect(buf[Size - 1]);
            for (size_t i = 1; i < Size - 1 - minSize; ++i) {
                rect1 = dom.union_(rect1, getRect(buf[i]));
                rect2 = dom.union_(rect2, getRect(buf[Size - 1 - i]));
                if (i >= minSize) {
                    S += dom.margin(rect1) + dom.margin(rect2);
                }
            }
            if (S < bestS || axis == 0) {
                bestS = S;
                bestAxis = axis;
            }
        }

        std::sort(buf.begin(), buf.end(), [&] (const E &a, const E &b) {
            return dom.cmp(bestAxis, getRect(a), getRect(b));
        });
        std::array<rect_t, Size> rects1, rects2;
        rects1[0] = getRect(buf[0]);
        rects2[Size - 1] = getRect(buf[Size - 1]);
        for (size_t i = 1; i < Size; ++i) {
            rects1[i] = dom.union_(rects1[i - 1], getRect(buf[i]));
            rects2[Size - 1 - i] = dom.union_(rects2[Size - i], getRect(buf[Size - 1 - i]));
        }
        size_t bestSplit = 0;
        measure_t bestArea(0), bestOverlap(0);
        for (size_t i = minSize; i < Size - 1 - minSize; ++i) {
            auto intersection = dom.intersect(rects1[i - 1], rects2[i]);
            auto overlap = intersection.has_value() ? dom.area(*intersection) : measure_t(0);
            auto area = dom.area(rects1[i - 1]) + dom.area(rects2[i]);
            if (std::tie(overlap, area) < std::tie(bestOverlap, bestArea) || bestSplit == 0) {
                bestSplit = i;
                bestOverlap = overlap;
                bestArea = area;
            }
        }
        return bestSplit;
    }

    Node splitLeaf(Node &node, T &&extra) {
        auto &bufs = *this;
        std::array<T, LeafSize + 1> &buf = bufs.objects;
        for (size_t i = 0; i < LeafSize; ++i) {
            buf[i] = std::move(node.objects[i]);
            node.objects[i].~T();
        }
        buf[LeafSize] = extra;

        auto &dom = tree.mDomain;
        if (!bufs.reinsertMask[0]) {
            pos_t center = dom.center(node.rect);
            // sort by descending distance from center. we want to have
            // the reinserted elements at the start of the buffer
            std::sort(buf.begin(), buf.end(), [&] (const T &a, const T &b) {
                return dom.dist(dom.center(dom.rect(a)), center) > dom.dist(dom.center(dom.rect(b)), center);
            });
            for (size_t i = LeafReinsert; i < LeafSize + 1; ++i) {
                new(&node.objects[i - LeafReinsert]) T(std::move(buf[i]));
            }
            node.size = buf.size() - LeafReinsert;
            bufs.reinserts = LeafReinsert;
            bufs.reinsertLevel = 0;
            return {0};
        }

        // reinsertion already done at this level, we have to split
        auto bestSplit = computeSplit(buf, [=] (const T &a) {return dom.rect(a);}, LeafMin);

        Node newNode;
        newNode.size = bestSplit;
        assert(newNode.size < LeafSize);
        newNode.objects = tree.allocateLeafArray();
        for (size_t i = 0; i < bestSplit; ++i) {
            new(&newNode.objects[i]) T(std::move(buf[i]));
        }
        tree.boundLeaf(newNode);
        node.size = buf.size() - bestSplit;
        assert(node.size < LeafSize);
        for (size_t i = bestSplit; i < buf.size(); ++i) {
            new(&node.objects[i - bestSplit]) T(buf[i]);
        }
        tree.boundLeaf(node);
        return newNode;
    }
    Node splitInner(Node &node, Node &&extra, level_t level) {
        auto &bufs = *this;
        // copy the node content and the new child into a buffer we can sort
        std::array<Node, InnerSize + 1> &buf = bufs.children;
        for (size_t i = 0; i < InnerSize; ++i) {
            buf[i] = std::move(node.children[i]);
            node.children[i].~Node();
        }
        buf[InnerSize] = extra;

        if (!bufs.reinsertMask[level]) {
            auto &dom = tree.mDomain;
            pos_t center = dom.center(node.rect);
            // sort by descending distance from center. we want to have
            // the reinserted elements at the start of the buffer
            std::sort(buf.begin(), buf.end(), [&] (const Node &a, const Node &b) {
                return dom.dist(dom.center(a.rect), center) > dom.dist(dom.center(b.rect), center);
            });
            for (size_t i = InnerReinsert; i < InnerSize + 1; ++i) {
                new(&node.children[i - InnerReinsert]) Node(std::move(buf[i]));
            }
            node.size = InnerSize + 1 - InnerReinsert;
            bufs.reinserts = InnerReinsert;
            bufs.reinsertLevel = level;
            return {0};
        }

        // reinsertion already done at this level, we have to split
        auto bestSplit = computeSplit(buf, [=] (const Node &a) {return a.rect;}, InnerMin);

        Node newNode;
        newNode.size = bestSplit;
        newNode.children = tree.allocateNodeArray();
        for (size_t i = 0; i < bestSplit; ++i) {
            new(&newNode.children[i]) Node(std::move(buf[i]));
        }
        tree.boundInner(newNode);
        node.size = InnerSize + 1 - bestSplit;
        for (size_t i = bestSplit; i < InnerSize + 1; ++i) {
            new(&node.children[i - bestSplit]) Node(std::move(buf[i]));
        }
        tree.boundInner(node);
        return newNode;
    }

    Node insertElement(T &&obj, const rect_t &rect, Node &node, level_t level) {
        // unnecessary if node has to be split or reinsertion occurs but detecting
        // these situations precisely would lead to messy control flow
        node.rect = tree.mDomain.union_(node.rect, rect);

        if (level > 0) {
            auto childIdx = level > 1 ? chooseInner(node, rect) : chooseLeaf(node, rect);
            auto &child = node.children[childIdx];

            // if the node is not full, we can make the next recursion level construct
            // a split node (if any) in-place. otherwise we may need to
            // split/reinsert ourselves
            if (node.size < InnerSize) {
                Node *newChild = &node.children[node.size];
                new(newChild) Node(insertElement(std::forward<T>(obj), rect, child, level - 1));
                if (newChild->size) {
                    node.size += 1;
                } else {
                    newChild->~Node();
                    if (this->reinserts) {
                        tree.boundInner(node);
                    }
                }
                return {0};
            }
            auto split = insertElement(std::forward<T>(obj), rect, child, level - 1);
            if (split.size) {
                return splitInner(node, std::move(split), level);
            } else if (this->reinserts) {
                tree.boundInner(node);
            }
            return {0};
        }

        if (node.size < LeafSize) {  // happy path: it just fits
            new(&node.objects[node.size++]) T(std::forward<T>(obj));
            return {0};
        }
        return splitLeaf(node, std::forward<T>(obj));
    }
    Node insertSubtree(Node &&ins, level_t insLevel, Node &node, level_t level) {
        node.rect = tree.mDomain.union_(node.rect, ins.rect);
        if (level <= insLevel) {
            if (node.size < InnerSize) {
                new(&node.children[node.size++]) Node(ins);
                return {0};
            }
            return splitInner(node, std::forward<Node>(ins), level);
        }
        auto childIdx = chooseInner(node, ins.rect);
        if (node.size < InnerSize) {
            Node *newChild = &node.children[node.size];
            new(newChild) Node(insertSubtree(std::forward<Node>(ins), insLevel, node.children[childIdx], level - 1));
            if (newChild->size) {
                node.size += 1;
            } else {
                newChild->~Node();
                if (this->reinserts) {
                    tree.boundInner(node);
                }
            }
            return {0};
        }
        auto split = insertSubtree(std::forward<Node>(ins), insLevel, node.children[childIdx], level - 1);
        if (split.size) {
            return splitInner(node, std::move(split), level);
        } else if (this->reinserts) {
            tree.boundInner(node);
        }
        return {0};
    }
    void splitRoot(Node &&split) {
        if (tree.mDepth == max_depth) {
            tree.destroySubtree(split, max_depth);
            throw std::length_error("exceeding max R* tree depth");
        }
        Node &root = tree.mRoot;
        Node oldRoot(std::move(root));
        root.size = 2;
        root.children = tree.allocateNodeArray();
        new(&root.children[0]) Node(std::move(oldRoot));
        new(&root.children[1]) Node(std::move(split));
        tree.boundInner(root);
        tree.mDepth += 1;
    }

    static void reinsertInner(RStar &parent) {
        RStar bufs(parent.tree, parent.reinsertMask);
        for (size_t i = 0; i < parent.reinserts; ++i) {
            Node split = bufs.insertSubtree(std::move(parent.children[i]), parent.reinsertLevel, bufs.tree.mRoot, bufs.tree.mDepth);
            if (split.size) {
                bufs.splitRoot(std::move(split));
            } else if (bufs.reinserts) {
                bufs.reinsertMask[bufs.reinsertLevel] = 1;
                reinsertInner(bufs);
                bufs.reinserts = 0;
            }
        }
    }
    static void insertLeaf(Tree &tree, T &&obj, ReinsertMask &mask) {
        auto &root = tree.mRoot;
        if (root.size == 0) {
            root.objects = tree.allocateLeafArray();
            root.rect = tree.mDomain.rect(obj);
        }
        RStar bufs(tree, mask);
        rect_t rect = tree.mDomain.rect(obj);
        Node split = bufs.insertElement(std::forward<T>(obj), rect, root, tree.mDepth);
        if (split.size) {
            bufs.splitRoot(std::move(split));
        } else if (bufs.reinserts) {
            bufs.reinsertMask[bufs.reinsertLevel] = 1;
            if (bufs.reinsertLevel == 0) {
                for (size_t i = 0; i < bufs.reinserts; ++i) {
                    insertLeaf(tree, std::move(bufs.objects[i]), bufs.reinsertMask);
                }
            } else {
                reinsertInner(bufs);
            }
            bufs.reinserts = 0;
        }
    }

public:
    static void insert(Tree &tree, T &&obj) {
        std::bitset<max_depth + 1> reinsertMask;
        insertLeaf(tree, std::forward<T>(obj), reinsertMask);
    }
};
