// SPDX-License-Identifier: MIT
#include "Combat.hh"
#include <cinttypes>

#include <typed-geometry/tg-std.hh>
#include <glow/common/log.hh>

#include <Util.hh>
#include <navmesh/NavMesh.hh>

using namespace Combat;

template<typename T>
static std::pair<T, T> accelStats(T va, T vb, T accel) {
    auto diff = vb - va;
    auto time = diff / accel;
    return {time, time * (va + .5f * diff * time)};
}

template<typename T>
static std::optional<std::pair<T, T>> solveQuadratic(T p, T q) {
    p *= T(.5);
    auto radicand = p * p - q;
    if (radicand < T(0)) {return {};}
    auto root = std::sqrt(radicand);
    return {{p - root, p + root}};
}

template<typename T>
static T maxEndVel(T vstart, T accel, T dist) {
    auto sol = solveQuadratic(vstart / accel, -dist / accel);
    TG_ASSERT(sol.has_value());
    auto [a, b] = Util::ordered(sol->first, sol->second);
    return accel * (a >= 0 ? a : b);
}

/// compute the time needed to travel from `prev` to `knot`
static double integrateTime(const MobileUnit::Knot &prev, const MobileUnit::Knot &knot) {
    auto dist = knot.knotPos - prev.knotPos;
    auto va = prev.velocity, vb = knot.velocity;

    // the polynomial going through (0, 0), with derivatives of va at 0 and vb at t
    // is va * x + .5 (vb - va) / t * x². Its value at t is thus va * t + .5 (vb - va) * t
    // = .5 (va + vb) * t. We want to find t such that the value of the polynomial is
    // the length of the leg, so we just need to solve that linear equation.
    // much simpler than I expected.
    auto avgVel = .5f * va + .5f * vb;
    float duration = dist / avgVel;
    return prev.time + duration;
}

bool MobileUnit::planRoute(std::pair<const ECS::entity, NavMesh::Instance> &navItem, const NavMesh::RouteRequest &req, ECS::ECS &ecs) {
    auto &mob = *this;
    mob.nav = navItem.first;
    auto &nav = navItem.second;
    mob.knots.clear();
    mob.knots.reserve(4);  // prevent resizing in same-face case
    auto &start = mob.knots.emplace_back();
    start.knotPos = start.velocity = 0.f;
    start.time = ecs.simSnap->worldTime;
    start.pos = req.start;
    auto timeToCruise = mob.cruiseSpeed / mob.acceleration;
    auto distToCruise = .5f * mob.cruiseSpeed * timeToCruise;
    if (req.start_face != req.end_face) {
        Obstacle::Collider collider(ecs, mob.heightVector, mob.radius);
        auto route = nav.navigate(req, 5, collider);
        for (auto &&c : route) {
            auto &cross = mob.knots.emplace_back();
            cross.he = c.first;
            auto he = nav.mesh->handle_of(c.first);
            auto a = nav.worldPos[he.vertex_from()];
            auto b = nav.worldPos[he.vertex_to()];
            cross.pos = tg::lerp(a, b, c.second);
            cross.velocity = mob.cruiseSpeed;
        }
    } else {
        // FIXME: test for collisions in this case
        auto distVec = req.end - req.start;
        auto dist = tg::length(distVec);
        glow::info() << "3-knot intra-face";
        auto &mid = mob.knots.emplace_back();
        mid.knotPos = .5f * dist;
        mid.pos = tg::lerp(req.start, req.end, .5f);
        mid.velocity = mob.cruiseSpeed;
    }
    if (knots.size() == 1) {
        glow::warning() << "no route found";
        return false;
    }
    {
        auto &end = mob.knots.emplace_back();
        end.velocity = 0.f;
        end.pos = req.end;
    }

    // limit acceleration rate
    for (size_t i = 1 ; i < mob.knots.size(); ++i) {
        const auto &prev = mob.knots[i - 1];
        auto &knot = mob.knots[i];
        auto dist = tg::distance(prev.pos, knot.pos);
        knot.knotPos = prev.knotPos + dist;
        auto maxVel = maxEndVel(prev.velocity, mob.acceleration, dist);
        knot.velocity = std::min(maxVel, knot.velocity);
    }
    // limit deceleration rate
    for (size_t i = mob.knots.size() ; i > 1; --i) {
        const auto &next = mob.knots[i - 1];
        auto &knot = mob.knots[i - 2];
        auto dist = next.knotPos - knot.knotPos;
        auto maxVel = maxEndVel(next.velocity, mob.acceleration, dist);
        knot.velocity = std::min(maxVel, knot.velocity);
    }
    // compute arrival times
    for (size_t i = 1; i < mob.knots.size(); ++i) {
        auto &knot = mob.knots[i];
        knot.time = integrateTime(mob.knots[i - 1], knot);
        glow::info() << knot.knotPos << " " << knot.pos << " t=" << knot.time << " v=" << knot.velocity;
    }
    return true;
}


std::pair<tg::pos3, tg::vec3> MobileUnit::interpolate(double time) const {
    auto &mob = *this;
    auto nextKnot = std::upper_bound(
        mob.knots.begin(), mob.knots.end(), time,
        [=] (double time, const MobileUnit::Knot &k) {return time < k.time;}
    ), curKnot = nextKnot;
    TG_ASSERT(curKnot != mob.knots.begin());
    --curKnot;

    if (nextKnot == mob.knots.end()) {  // destination already reached
        return {curKnot->pos, tg::vec3::zero};
    }
    auto va = curKnot->velocity, vb = nextKnot->velocity;
    auto quadFact = .5f * (vb - va) / (nextKnot->time - curKnot->time);
    auto x = time - curKnot->time;
    float dist = nextKnot->knotPos - curKnot->knotPos;
    auto fwd = (nextKnot->pos - curKnot->pos) / dist;
    float param = x * (va + x * quadFact);
    auto pos = curKnot->pos + param * fwd;
    fwd *= va + 2 * quadFact * x;
    return {pos, fwd};
}

std::optional<std::pair<double, double>> MobileUnit::timeRange() const {
    if (knots.empty()) {return {};}
    return {{knots.front().time, knots.back().time}};
}
