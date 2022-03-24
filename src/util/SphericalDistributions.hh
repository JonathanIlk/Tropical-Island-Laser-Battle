// SPDX-License-Identifier: MIT
#pragma once
#include <random>

#include <typed-geometry/tg-lean.hh>
#include <typed-geometry/functions/basic/scalar_math.hh>

namespace Util {
template<typename ScalarT>
const ScalarT afterOne = std::nextafter(ScalarT(1), std::numeric_limits<ScalarT>::max());
}

template<typename ScalarT = float>
class UnitCircleDistribution {
    std::uniform_real_distribution<ScalarT> angle{0, 360_deg .radians()};

public:
    template<typename Generator>
    tg::dir<2, ScalarT> operator()(Generator &rng) {
        auto [sin, cos] = tg::sin_cos(tg::angle_t<ScalarT>::from_radians(angle(rng)));
        return {cos, sin};
    }
};
template<typename ScalarT = float>
class UnitDiscDistribution {
    std::uniform_real_distribution<ScalarT> reals {
        0.f, Util::afterOne<ScalarT>
    };
    UnitCircleDistribution<ScalarT> circle;

public:
    template<typename Generator>
    tg::vec<2, ScalarT> operator()(Generator &rng) {
        auto radius = std::sqrt(reals(rng));
        return circle(rng) * radius;
    }
};
template<typename ScalarT = float>
class UnitSphereDistribution {
    std::uniform_real_distribution<ScalarT> reals {
        -1, Util::afterOne<ScalarT>
    };
    UnitCircleDistribution<ScalarT> circle;

public:
    template<typename Generator>
    tg::dir<3, ScalarT> operator()(Generator &rng) {
        auto z = reals(rng);
        auto radius = std::sqrt(ScalarT(1) - z * z);
        return tg::dir<3, ScalarT>(circle(rng) * radius, z);
    }
};
