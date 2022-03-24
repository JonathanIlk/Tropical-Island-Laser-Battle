// SPDX-License-Identifier: MIT
#pragma once
#include "Combat.hh"

namespace Combat {
struct KeyFrame {
    double time;
    const Stance *stance;
    ECS::Rigid base;
    ECS::Rigid gun;
    float velocity;
};

struct Stepper {
    const MobileUnit &mob;
    const Humanoid &hum;
    tg::vec3 up;
    unsigned steps;
    double start, end, stepTime;

    Stepper(const MobileUnit &mob, const Humanoid &hum, tg::vec3 up, float stepsPerSecond) : mob{mob}, hum{hum}, up{up} {
        // assume existence of knots has been checked by caller
        auto timeRange = mob.timeRange();
        TG_ASSERT(timeRange.has_value());
        std::tie(start, end) = *timeRange;
        auto duration = end - start;
        steps = std::ceil(duration * stepsPerSecond);
        stepTime = steps ? duration / steps : 0.f;
    }

    unsigned stepFromTime(double time) const {
        return (time - start) / stepTime;
    }
    KeyFrame getStep(unsigned step) const;
};

struct MovementContext {
    ECS::ECS &ecs;
    Humanoid &hum;
    MobileUnit &mob;
    NavMesh::Instance &nav;

    void setFeet(HumanoidPos &pos, const Stance &stance) const;

    void interpolate(HumanoidPos &humpos, double time) const;
    HumanoidPos posFromKeyFrame(const KeyFrame &) const;
    HumanoidPos restPos(const ECS::Rigid &) const;
    void planWalk(const HumanoidPos &startPos, const tg::dir3 &up, const tg::vec3 &endFwd);
};

}
