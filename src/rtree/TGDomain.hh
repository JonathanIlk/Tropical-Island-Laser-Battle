#pragma once
#include <optional>
#include <tuple>

#include <typed-geometry/tg.hh>

template<size_t D, typename ScalarT>
struct TGDomain {
    using pos_t = tg::pos<D, ScalarT>;
    using rect_t = tg::aabb<D, ScalarT>;
    using measure_t = ScalarT;
    using margin_t = ScalarT;
    using distance_t = ScalarT;
    static constexpr size_t dimension = D;

    std::optional<rect_t> intersect(const rect_t &a, const rect_t &b) const {
        auto min = tg::max(a.min, b.min);
        auto max = tg::min(a.max, b.max);
        for (size_t i = 0; i < D; ++i) {
            if (min[i] >= max[i]) {return std::nullopt;}
        }
        return {{min, max}};
    }
    rect_t union_(const rect_t &a, const rect_t &b) const {
        return {tg::min(a.min, b.min), tg::max(a.max, b.max)};
    }

    measure_t area(const rect_t &v) const {
        ScalarT res(1);
        for (size_t i = 0; i < D; ++i) {res *= v.max[i] - v.min[i];}
        return res;
    }
    margin_t margin(const rect_t &v) const {
        ScalarT res(0);
        for (size_t i = 0; i < D; ++i) {res += v.max[i] - v.min[i];}
        return res;
    }

    bool cmp(size_t axis, const rect_t &a, const rect_t &b) const {
        return std::tie(a.min[axis], a.max[axis]) < std::tie(b.min[axis], b.max[axis]);
    }
    margin_t getMin(size_t axis, const rect_t &a) const {
        return a.min[axis];
    }
    margin_t getMax(size_t axis, const rect_t &a) const {
        return a.max[axis];
    }

    pos_t center(const rect_t &a) const {
        return tg::lerp(a.min, a.max, ScalarT(.5));
    }
    distance_t dist(const pos_t &a, const pos_t &b) const {
        return tg::distance_sqr(a, b);
    }

    template<typename T>
    rect_t rect(const T &a) const {return a.getAABB();}
    rect_t rect(const tg::pos<D, ScalarT> &a) const {return {a, a};}
    rect_t rect(const tg::segment<D, ScalarT> &a) const {
        return {tg::min(a.pos0, a.pos1), tg::max(a.pos0, a.pos1)};
    }
    rect_t rect(const tg::aabb<D, ScalarT> &a) const {return a;}
};
