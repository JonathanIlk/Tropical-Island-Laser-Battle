// SPDX-License-Identifier: MIT
#include "Walking.hh"
#include <MathUtil.hh>
#include <Util.hh>
#include <navmesh/NavMesh.hh>
#include <obstacles/Obstacle.hh>

using namespace Combat;

KeyFrame Stepper::getStep(unsigned step) const {
    TG_ASSERT(step > 0 && step < steps);
    auto time = start + step * stepTime;
    auto [pos, vel] = mob.interpolate(time);
    return {
        time, &hum.stepStance,
        {pos, Util::upForwardOrientation(up, vel)},
        hum.stepGunPos[step % 2],
        tg::length(vel)
    };
}

static ECS::Rigid footRay(const tg::ray3 &ray, const NavMesh::Instance &nav, tg::quat &rot, float maxLen) {
    auto ints = nav.intersect(ray);
    if (!ints || ints->second > maxLen) {
        // no ground to put feet on, just step on the air
        return {ray[maxLen], rot};
    } else {
        return {ray[ints->second], Util::upForwardOrientation(
            nav.faceNormal(ints->first), rot * tg::dir3(0, 0, -1)
        )};
    }
}

void MovementContext::setFeet(HumanoidPos &pos, const Stance &stance) const {
    auto hip = pos.upperBody * pos.hip;
    for (auto foot : Util::IntRange(0, 2)) {
        auto rot = pos.base.rotation * tg::quat::from_axis_angle({0, 1, 0}, stance.feet[foot].angle);
        auto dir = stance.feet[foot].dir;
        tg::ray3 ray {
            hip * tg::pos3((foot == 0 ? .5f : -.5f) * hum.hipJointDist, 0, 0),
            pos.base.rotation * dir
        };
        pos.feet[foot] = footRay(ray, nav, rot, hum.hipHeight);
    }
}

void MovementContext::interpolate(HumanoidPos &humpos, double time) const {
    size_t curStep;
    const auto &steps = hum.steps;
    {
        auto next = std::upper_bound(
            steps.begin(), steps.end(),
            time, [] (double a, const Step &b) {return a < b.time;}
        );
        if (next == steps.begin()) {
            TG_ASSERT(next != steps.end());
            humpos = next->pos;
            return;
        } else if (next == steps.end()) {
            TG_ASSERT(next != steps.begin());
            --next;
            humpos = next->pos;
            return;
        }
        curStep = std::distance(steps.begin(), next) - 1;
    }
    auto &prev = steps[curStep], &next = steps[curStep + 1];
    float param = (time - prev.time) / (next.time - prev.time);
    param = std::clamp(param, 0.f, 1.f);
    humpos.base = prev.pos.base.interpolate(next.pos.base, param);
    auto up = humpos.base * tg::dir3(0, 1, 0);

    auto x = param - .5f;
    auto bounce = .1f - .4f * x * x;
    bounce *= tg::distance(next.pos.base.translation, prev.pos.base.translation);
    humpos.upperBody = prev.pos.upperBody.interpolate(next.pos.upperBody, param);
    humpos.upperBody.translation += up * bounce;
    humpos.hip = prev.pos.hip.interpolate(next.pos.hip, param);
    humpos.gun = prev.pos.gun.interpolate(next.pos.gun, param);
    humpos.head = prev.pos.head.interpolate(next.pos.head, param);

    {
        size_t firstFoot = 0;
        auto bothFeet = Util::IntRange(size_t(2));
        bool firstStep = curStep == 0, lastStep = curStep >= steps.size() - 2;
        struct FootContact {double time; ECS::Rigid pos;float velocity;} key[4];
        auto getKey = [&] (size_t step) {
            return FootContact({
                steps[step].time, steps[step].pos.feet[step % 2], steps[step].velocity
            });
        };
        key[1] = getKey(curStep);
        key[2] = getKey(curStep + 1);
        key[3] = lastStep ? key[2] : getKey(curStep + 2);
        if (firstStep) {
            key[0] = key[1];
            key[0].pos = steps[0].pos.feet[1 ^ firstFoot];
        } else {
            key[0] = getKey(curStep - 1);
        }
        if (lastStep) {
            key[3] = key[2];
            key[3].pos = steps.back().pos.feet[(steps.size() % 2) ^ 1];
        }
        for (auto i : bothFeet) {
            auto dt = key[i + 2].time - key[i].time;
            if (i == 0 && firstStep) {
                key[2].time -= dt / 2;
            } else if (i == 1 && lastStep) {
                key[1].time += dt / 2;
            } else {
                // airtime for normal steps: up to twice the stride length, half of the
                // foot movement time is spent on the ground, leading to 0 airtime.
                auto dist = tg::distance(key[i].pos.translation, key[i + 2].pos.translation);
                if (dist <= 2 * hum.strideLength) {
                    key[i].time += .25f * dt;
                    key[i + 2].time -= .25f * dt;
                } else {
                    auto groundTime = dt * hum.strideLength / dist;
                    if (key[i + 2].velocity > key[i].velocity) {
                        key[i].time += groundTime;
                    } else if (key[i + 2].velocity > key[i].velocity) {
                        key[i].time += .5f * groundTime;
                        key[i + 2].time -= .5f * groundTime;
                    } else {
                        key[i].time += .7f * groundTime;
                        key[i + 2].time -= .3f * groundTime;
                    }
                }
            }
        }
        for (auto i : bothFeet) {
            size_t idx = curStep % 2 ^ i;
            auto &out = humpos.feet[i ^ 1 ^ firstFoot];
            auto timeA = key[idx].time;
            float dt = key[idx + 2].time - timeA;
            if (dt > 0) {
                auto param = std::clamp(float(time - timeA) / dt, 0.f, 1.f);
                auto dist = tg::distance(key[idx].pos.translation, key[idx + 2].pos.translation);
                auto bounce = param - .5f;
                bounce = -4.f * bounce * bounce + 1.f;
                bounce *= std::clamp(dist / 4, 0.f, hum.hipHeight / 2);
                out.translation = tg::lerp(
                    key[idx].pos.translation, key[idx + 2].pos.translation, param
                ) + bounce * up;
                out.rotation = tg::slerp(
                    key[idx].pos.rotation, key[idx + 2].pos.rotation, param
                );
            } else {
                out = key[idx].pos;
            }
        }
    }
}

ECS::Rigid Stance::upperBody() const {
    return {tg::pos3(0, height, 0), upperBodyOrient};
}

HumanoidPos HumanoidPos::fromStance(const Stance &stance, const ECS::Rigid &base) {
    return {
        base, base * stance.upperBody(), {{0, 0, 0}, stance.hipOrient}
    };
}

HumanoidPos MovementContext::posFromKeyFrame(const KeyFrame &key) const {
    auto res = HumanoidPos::fromStance(*key.stance, key.base);
    setFeet(res, *key.stance);
    res.gun = key.gun;
    res.head = {{0, hum.axisHeight - hum.hipHeight, 0}};
    return res;
}

HumanoidPos MovementContext::restPos(const ECS::Rigid &base) const {
    const Stance *stance = &hum.baseStance;
    auto closest = ecs.obstacleSys->closest(base.translation);
    if (closest && closest->second < hum.lowCoverDistance) {
        auto iter = ecs.obstacles.find(closest->first);
        if (iter != ecs.obstacles.end() && !iter->second.highCover) {
            stance = &hum.crouchStance;
        }
    }
    auto res = HumanoidPos::fromStance(*stance, base);
    setFeet(res, *stance);
    res.gun = {hum.gunCenter + tg::vec3(0, 0, -hum.gunOffset), tg::conjugate(res.upperBody.rotation) * res.base.rotation};
    res.head = {{0, hum.axisHeight - hum.hipHeight, 0}, res.gun.rotation};
    return res;
}

void MovementContext::planWalk(const HumanoidPos &startPos, const tg::dir3 &up, const tg::vec3 &endFwd) {
    TG_ASSERT(!mob.knots.empty());
    Stepper stepper(mob, hum, up, hum.stepsPerSecond);
    hum.steps.clear();
    hum.steps.push_back({stepper.start, 0.f, startPos});
    for (auto i : Util::IntRange(unsigned(1), stepper.steps)) {
        auto key = stepper.getStep(i);
        hum.steps.push_back(
            {key.time, key.velocity, posFromKeyFrame(key)}
        );
    }
    hum.steps.push_back({stepper.end, 0.f, restPos(
        {mob.knots.back().pos, Util::upForwardOrientation(up, endFwd)}
    )});
}

namespace {
template<typename ScalarT>
tg::angle_t<ScalarT> angleFromSides(ScalarT a, ScalarT asq, ScalarT b, ScalarT bsq, ScalarT csq) {
    // rearranged form of the cosine rule
    return tg::acos(std::clamp((asq + bsq - csq) * ScalarT(.5) / (a * b), ScalarT(-1), ScalarT(1)));
}

template<typename ScalarT>
tg::vec<3, ScalarT> referenceVector(const tg::vec<3, ScalarT> &vec, const tg::dir<3, ScalarT> &fwd, const tg::dir<3, ScalarT> &altFwd) {
    auto cross = tg::cross(fwd, vec);
    auto crossLensq = tg::length_sqr(cross);
    auto thresh = ScalarT(.01) * tg::length_sqr(vec);
    return crossLensq >= thresh ? fwd : tg::lerp(
        tg::vec<3, ScalarT>(altFwd), tg::vec<3, ScalarT>(fwd), crossLensq / thresh
    );
}
}

LegPos HumanoidPos::legPos(int leg, const Humanoid &hum) const {
    TG_ASSERT(leg < 2);
    auto hipJoint = (upperBody * this->hip) * tg::pos3((leg == 0 ? .5f : -.5f) * hum.hipJointDist, 0, 0);
    auto foot = feet[leg];
    auto anklePos = foot * tg::pos3(0, hum.ankleHeight, 0);
    auto footVec = anklePos - hipJoint;
    auto ankleDistsq = tg::length_sqr(footVec), ankleDist = std::sqrt(ankleDistsq);
    auto limbLength = hum.hipHeight - hum.ankleHeight;
    auto thighLength = hum.hipHeight - hum.kneeHeight;
    tg::angle32 thighAngle;
    if (ankleDist > limbLength) {
        auto newFootVec = (limbLength / ankleDist) * footVec;
        foot.translation += newFootVec - footVec;
        ankleDist = limbLength, ankleDistsq = Util::pow2(limbLength);
        thighAngle = 0_deg;
    } else {
        auto lowerLegLength = hum.kneeHeight - hum.ankleHeight;
        thighAngle = angleFromSides(
            thighLength, Util::pow2(thighLength),
            ankleDist, ankleDistsq,
            Util::pow2(lowerLegLength)
        );
    }

    auto mat = tg::mat3(foot.rotation);
    auto baseOrient = Util::upForwardOrientation(
        -footVec, referenceVector(-footVec, tg::dir3(-mat[2]), tg::dir3(mat[1]))
    );

    ECS::Rigid thigh = {hipJoint, baseOrient * tg::quat::from_axis_angle(
        {1, 0, 0}, thighAngle
    )};

    ECS::Rigid lowerLeg = {thigh * tg::pos3(0, -thighLength, 0)};
    auto right = baseOrient * tg::dir3(1, 0, 0);
    auto lowerLegUp = tg::normalize(lowerLeg.translation - anklePos);
    auto lowerLegFwd = tg::cross(lowerLegUp, right);
    lowerLeg.rotation = tg::quat::from_rotation_matrix({
        tg::vec3(right), tg::vec3(lowerLegUp), -lowerLegFwd
    });

    return {thigh, lowerLeg, foot};
}

ArmPos HumanoidPos::armPos(int side, const ECS::Rigid &chest, const Humanoid &hum, const ECS::Rigid &hand) const {
    TG_ASSERT(side < 2);

    float sign = side == 0 ? 1 : -1;
    auto shoulderPos =  chest * tg::pos3(sign * .5f * hum.shoulderDist, 0, 0);
    auto handVec = hand.translation - shoulderPos;
    auto handDistsq = tg::length_sqr(handVec), handDist = std::sqrt(handDistsq);
    tg::angle32 angle;
    if (handDist > hum.armLength) {
        handVec *= hum.armLength / handDist;
        handDist = hum.armLength, handDistsq = Util::pow2(hum.armLength);
        angle = 0_deg;
    } else {
        auto lowerArmLength = hum.armLength - hum.elbowPos;
        angle = angleFromSides(
            hum.elbowPos, Util::pow2(hum.elbowPos),
            handDist, handDistsq,
            Util::pow2(lowerArmLength)
        );
    }
    auto ref = tg::normalize(tg::vec3(-1, 2, 1)), ref2 = tg::normalize(tg::vec3(-2, -1, 1));
    ref.x *= sign; ref2.x *= sign;
    ref = chest * ref, ref2 = chest * ref2;
    auto baseOrient = Util::rightForwardOrientation(
        sign * handVec, referenceVector(handVec, ref, ref2)
    );
    ECS::Rigid upperArm {
        shoulderPos, baseOrient * tg::quat::from_axis_angle({0, 1, 0}, -sign * angle)
    };
    auto elbow = upperArm * tg::pos3(sign * hum.elbowPos, 0, 0);
    auto up = upperArm.rotation * tg::vec3(0, 1, 0);
    auto lowerArmRight = sign * tg::normalize(hand.translation - elbow);
    ECS::Rigid lowerArm {elbow, tg::quat::from_rotation_matrix(tg::mat3(
        lowerArmRight, up, tg::cross(lowerArmRight, up)
    ))};
    return {upperArm, lowerArm, hand};
}
