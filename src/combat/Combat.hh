// SPDX-License-Identifier: MIT
#pragma once
#include <random>
#include <vector>

#include <typed-geometry/tg-lean.hh>
#include <polymesh/cursors.hh>
#include <glow/fwd.hh>

#include <ECS.hh>
#include <ECS/Misc.hh>
#include <effects/Effects.hh>
#include <animation/rigged/RiggedMesh.hh>

namespace Combat {

struct MobileUnit {
    tg::vec3 heightVector;
    float cruiseSpeed, acceleration, radius;

    ECS::entity nav;

    struct Knot {
        tg::pos3 pos;
        pm::halfedge_index he;  ///< invalid if the Knot is a manually-given waypoint
        float knotPos;  ///< cumulative length of the linear segments up to here
        // planned velocity when passing this point. Should be 0 at both start
        // and end; if there is no crossing inbetween 0-velocity points, 1 or 2
        // EndPoints with positive velocity need to be inserted inbetween
        // (with velocity according to `cruiseSpeed` and `acceleration`)
        float velocity;
        double time;  // planned time at which this point is reached
    };
    std::vector<Knot> knots;

    bool planRoute(std::pair<const ECS::entity, NavMesh::Instance> &navItem, const NavMesh::RouteRequest &req, ECS::ECS &ecs);
    std::pair<tg::pos3, tg::vec3> interpolate(double time) const;
    std::optional<std::pair<double, double>> timeRange() const;
};

struct Stance {
    float height;
    tg::quat hipOrient, upperBodyOrient;
    struct Foot {
        tg::dir3 dir;
        tg::angle32 angle;
    } feet[2];

    ECS::Rigid upperBody() const;
};

struct LegPos {
    ECS::Rigid thigh, lowerLeg, foot;
};
struct ArmPos {
    ECS::Rigid upperArm, lowerArm, hand;
};

struct HumanoidPos {
    /// `base` is not used for rendering, but for various logic:
    /// its translation is the unit's canonical position concerning navigation,
    /// its rotation can be used to derive the canonical 'forward' direction
    /// (e. g. for line-of-sight purposes), or an 'up' vector whenever the orientation
    /// of the navmesh is not available
    ECS::Rigid base;

    ECS::Rigid upperBody;
    ECS::Rigid hip, gun, head;  // relative to upperBody
    ECS::Rigid feet[2];

    static HumanoidPos fromStance(const Stance &, const ECS::Rigid &base);
    LegPos legPos(int leg, const Humanoid &hum) const;
    ArmPos armPos(int side, const ECS::Rigid &chest, const Humanoid &hum, const ECS::Rigid &hand) const;
};

struct HumanoidRenderInfo {
    ECS::entity id;
    const Humanoid &hum;
    ECS::Rigid hip, gun, chest, head;
    LegPos legs[2];
    ArmPos arms[2];
};

struct Step {
    double time;
    float velocity;
    HumanoidPos pos;
};

struct ParametricHumanoid;

struct Humanoid {
    std::vector<Step> steps;

    // === combat
    int allegiance = 0;  // 0 = player, everything else is enemy
    ECS::entity curTarget = ECS::INVALID;

    double shotReadyAt = 0.;
    float cooldown = 2.f;
    int attackDamage = 40;
    int hp = 100, maxHP = 100;

    float attackRange = 30.f, attackCos = tg::cos(45_deg);
    tg::angle32 turningSpeed = 270_deg;  // per second

    // === walking cycle parameters
    float strideLength = .3f, stepsPerSecond = 3.f;

    // === rig parameters
    float hipJointDist = .2f, shoulderDist = .38f;
    // the heights (and orientation of the corresponding bones) are based on
    // the T-Pose, where the ankle, knee and hip joints of each leg are on an
    // exactly vertical line
    float hipHeight = .95f, kneeHeight = .5f, ankleHeight = .09f;
    float shoulderHeight = 1.6f, axisHeight = 1.75f;  // as in axis vertebra
    // similar for the arms: the rig is based on the shoulder, elbow and wrist joint
    // being along one line parallel to the X axis, palms down (i. e. towards -Y)
    float armLength = .55f;  // shoulder-to-wrist
    float elbowPos = .3f;  // shoulder-to-elbow
    // see below for where each bone is anchored

    // === visuals
    Effects::ScatterLaser::Params scatterLaserParams;

    // replace this when transitioning to a skinned model
    std::unique_ptr<ParametricHumanoid> visual;

    // === pose parameters
    ECS::Rigid rightHandPos, leftHandPos, pumpHandPos;  ///< relative to gun

    /// relative to upperBody; this is where attacks are aimed at
    tg::pos3 bodyCenter;
    /// relative to upperBody; this is what the gun rotates around in attack mode
    /// and from where attack range is computed
    tg::pos3 gunCenter;
    float gunOffset = .75f;

    float lowCoverDistance = 2.f;

    Stance baseStance, stepStance, crouchStance;
    ECS::Rigid stepGunPos[2];

    Humanoid();
};

struct ParametricHumanoid {
    // hip: anchored at the midpoint between hip joints
    // T-Pose coords: (0, hipHeight, 0)
    tg::size3 pelvicSize = {.4f, .2f, .15f};
    tg::pos2 hipAnchor = {.1f, .1f};  // in YZ plane

    // head: anchored at axis vertebra
    // T-Pose coords: (0, axisHeight, 0)
    tg::size3 craniumSize {.13f, .16f, .18f};
    tg::pos2 headAnchor {.02f, -.1f};
    tg::pos2 chin {-.03f, -.19f};
    tg::pos3 jawEnd {.07f, 0, -.11f};

    // foot: anchored at the sole, under the ankle joint
    // T-Pose coords: (±hipJointDist / 2, 0, 0)
    // +Y: anchor → ankle joint (dist `ankleHeight`)
    // -Z: anchor → load-bearing center of the pads
    tg::size3 footSize = {.1f, .09f, .26f};
    tg::vec2 ankleJoint {.04f, .2f}, contactPoint {.02f, .07f};  // in XZ plane

    // lower leg: anchored at the knee joint
    // T-Pose coords: (±hipJointDist / 2, kneeHeight, 0)
    tg::size2 kneeSize = {.1f, .15f};

    // thigh: anchored at the hip joint
    // T-Pose coords: (±hipJointDist / 2, hipHeight, 0)
    float thighGap = .02f;

    // gun: anchored at the muzzle
    // (not present in T-Pose)
    // -Z: shooting direction
    tg::size3 gunSize = {.06f, .12f, .5f};
    tg::vec3 gunAnchor = {.03f, .06f, 0};

    // chest: anchored at the midpoint between the shoulder joints
    // T-Pose coords: (0, shoulderHeight, 0)
    tg::size2 waistSize = {.32f, .15f};  // XZ plane
    tg::pos2 chestAnchor {-.05f, -.1f};  // YZ plane
    float waistOffset = .03f, chestHeight = -.2f;
    tg::size3 chestSize = {.33f, .36f, .2f};

    // upper arm: anchored at the shoulder joint
    // T-Pose coords: (±shoulderDist / 2, shoulderHeight, 0)
    tg::aabb2 shoulderEnd {{-.05f, -.05f}, {.05f, .05f}};  // YZ plane
    tg::aabb2 elbowEnd {{-.05f, -.02f}, {.05f, .06f}};  // YZ plane

    // lower arm: anchored at the elbow
    // T-Pose coords: (±(shoulderDist / 2 + elbowPos), shoulderHeight, 0)
    tg::aabb2 wristEnd {{-.02f, -.025f}, {.02f, .04f}};  // YZ plane

    // hand: anchored at the wrist
    // T-Pose coords (±(shoulderDist / 2 + armLength), shoulderHeight, 0)
    tg::aabb2 padsEnd {{-.005f, -.04f}, {.02f, .04f}};  // YZ plane
    float palmLength = .1f, fingerLength = .09f;

    glow::SharedVertexArray vaoHip, vaoChest, vaoGun, vaoHead;
    struct Side {
        glow::SharedVertexArray vaoFoot, vaoThigh, vaoLowerLeg;
        glow::SharedVertexArray vaoUpperArm, vaoLowerArm, vaoHand;
    } side[2];

    ParametricHumanoid(const Humanoid &hum) {update(hum);}
    void update(const Humanoid &);
};

class System final : public ECS::Editor {
    Game &mGame;  // needs `Game` to set the active tool
    glow::SharedProgram mSimpleShader, mRiggedShader, mGaugeShader;
    RiggedMesh::Data mGoodGuyMesh;
    RiggedMesh::Data mBadGuyMesh;
    glow::SharedVertexArray mShotgunVao, mHPGaugeVao, mPathVao;
    glow::SharedArrayBuffer mPathABO;
    std::vector<size_t> mPathRanges;

public:
    System(Game &game);
    void editorUI(ECS::entity) override;

    bool lineOfSight(const tg::pos3 &pos, const tg::vec3 &distVec) const;

    void extrapolate(ECS::Snapshot &prev, ECS::Snapshot &next);
    void update(ECS::Snapshot &prev, ECS::Snapshot &next);
    void prepareRender(ECS::Snapshot &snap);
    void prepareUI(ECS::Snapshot &snap);
    void renderMain(MainRenderPass &);
    void renderUI(MainRenderPass &);

    void spawnSquad(NavMesh::Instance &nav, unsigned units, float radius, std::mt19937 &);
};

}
