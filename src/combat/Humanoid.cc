// SPDX-License-Identifier: MIT
#include "Combat.hh"
#include <algorithm>

#include <typed-geometry/tg-std.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/VertexArray.hh>

#include <MathUtil.hh>
#include <Util.hh>

using namespace Combat;

namespace {
static constexpr uint64_t END = 0xffff;
struct MiniMesh {
    using index = std::uint16_t;
    static constexpr index END = ~index(0);
    std::vector<tg::pos3> vertices;
    std::vector<index> indices;

    void mirror(const tg::vec3 &axis = {1, 0, 0}, float offset = 0.f) {
        auto factor = 2 / tg::length_sqr(axis);
        for (auto &v : vertices) {
            v -= axis * (factor * (tg::dot(v, axis) - offset));
        }
        auto iter = indices.begin();
        while (iter != indices.end()) {
            ++iter;
            auto end = std::find(iter, indices.end(), END);
            std::reverse(iter, end);
            iter = end;
            if (iter != indices.end()) {++iter;}
        }
    }

    void symmetric(index center, const tg::vec3 &axis = {1, 0, 0}, float offset = 0.f) {
        TG_ASSERT(center < vertices.size());
        index nSide = vertices.size() - center;
        auto factor = 2 / tg::length_sqr(axis);
        for (auto i : Util::IntRange(std::size_t(center), vertices.size())) {
            auto &v = vertices[i];
            vertices.push_back(v - axis * (factor * (tg::dot(v, axis) - offset)));
        }
        if (indices.size() && indices.back() != END) {indices.push_back(END);}
        std::size_t i = 0, end = indices.size();
        while (i < end) {
            auto iter = indices.begin() + i;
            auto end = std::find(iter, indices.end(), END);
            std::size_t j = std::distance(indices.begin(), end);
            if (std::any_of(iter, end, [center] (index idx) {return idx >= center;})) {
                auto v = *iter;
                indices.push_back(v >= center ? v + nSide : v);
                for (auto k = j - 1; k != i; --k) {
                    auto v = indices[k];
                    indices.push_back(v >= center ? v + nSide : v);
                }
                indices.push_back(END);
            }
            i = j + 1;
        }
    }

    glow::SharedVertexArray makeVAO() const {
        return glow::VertexArray::create(
            glow::ArrayBuffer::create("aPosition", vertices),
            glow::ElementArrayBuffer::create(indices),
            GL_TRIANGLE_FAN
        );
    }
};

MiniMesh generatePelvis(tg::size3 &pelvicSize, tg::pos2 &hipAnchor, float hipJointDist) {
    float hipJointX = hipJointDist / 2;
    MiniMesh res {{
        // front
        {0.2f * hipJointX, 0, 0},
        {0.2f * hipJointX, pelvicSize.height, 0},
        // side
        {.35f * pelvicSize.width, pelvicSize.height, .0f * pelvicSize.depth},
        {.45f * pelvicSize.width, pelvicSize.height, .1f * pelvicSize.depth},
        {.5f * pelvicSize.width, pelvicSize.height, .2f * pelvicSize.depth},
        {.45f * pelvicSize.width, pelvicSize.height, .7f * pelvicSize.depth},
        {.4f * pelvicSize.width, pelvicSize.height, .9f * pelvicSize.depth},
        {.3f * pelvicSize.width, pelvicSize.height, 1.f * pelvicSize.depth},
        // tailbone
        {0.50f * hipJointX, 1.f * pelvicSize.height, 1.0f * pelvicSize.depth},
        {0.42f * hipJointX, .8f * pelvicSize.height, .97f * pelvicSize.depth},
        {0.36f * hipJointX, .6f * pelvicSize.height, .90f * pelvicSize.depth},
        {0.28f * hipJointX, .4f * pelvicSize.height, .80f * pelvicSize.depth},
        {0.20f * hipJointX, .2f * pelvicSize.height, .67f * pelvicSize.depth},
        {0.12f * hipJointX, .0f * pelvicSize.height, .50f * pelvicSize.depth}
    }, {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, END,
        8, 7, 6, 5, 4, 3, 2, 1, END,
    }};
    for (auto &v : res.vertices) {v.y -= hipAnchor.x; v.z -= hipAnchor.y;}
    size_t nSide = res.vertices.size();
    TG_ASSERT(nSide == 14);
    res.symmetric(0);

    // stitch sides together
    uint16_t ring[] = {1, 0, 13, 12, 11, 10, 9, 8, 1};
    for (auto i : Util::IntRange(size_t(0), ARRAY_SIZE(ring) - 1)) {
        auto a = ring[i], b = ring[i + 1];
        decltype(a) a_ = a + nSide, b_ = b + nSide;
        res.indices.insert(res.indices.end(), {a, b, b_, a_, END});
    }
    return res;
}

MiniMesh generateFoot(const tg::size3 &size, const tg::vec2 &contactPoint, const tg::vec2 &ankleJoint) {
    std::vector<tg::pos3> vertices {
        // 0–4: toes
        {0.f, 0.f, contactPoint.y},
        {.25f * contactPoint.x, 0.f, 0.f},
        {1.25f * contactPoint.x, 0.f, 0.f},
        {.5f * size.width, 0.f, .2f * contactPoint.y},
        {size.width, 0.f, .8f * contactPoint.y},

        // 5–9: outer side, heel
        {size.width, 0.f, .5f * size.depth},
        {.8f * size.width, 0.f, .9f * size.depth},
        {.6f * size.width, 0.f, size.depth},
        {.3f * size.width, 0.f, size.depth},
        {.1f * size.width, 0.f, .9f * size.depth},

        // 10–14: top side
        {.5f * contactPoint.x, .4f * size.height, contactPoint.y},
        {0.f, size.height, ankleJoint.y},
        {.5f * size.width, size.height, .95f * size.depth},
        {.8f * size.width, size.height, ankleJoint.y},
        {ankleJoint.x, size.height, ankleJoint.y - .1f * size.depth}
    };
    TG_ASSERT(vertices.size() == 15);
    // give the toes some height
    for (auto i : Util::IntRange(1, 5)) {
        vertices.push_back(vertices[i]);
        vertices[i].y += .125f * size.height;
    }
    std::vector<uint16_t> indices {
        0, 15, 16, 17, 18, 5, 6, 7, 8, 9, END,  // bottom
        1, 2, 16, 15, 0, END, 3, 4, 18, 17, 16, 2, END, 4, 5, 18, END, // front
        10, 4, 3, 2, 1, 0, 11, 14, 4, END,  // toes, top
        13, 6, 5, 4, 14, 11, 12, 6, END,  // top, outer side
        12, 11, 9, 8, 7, 6, END,  // back
        11, 0, 9, END  // inner side
    };

    tg::vec3 offset = tg::vec3(-ankleJoint.x, 0, -ankleJoint.y);
    for (auto &v : vertices) {v += offset;}
    return {std::move(vertices), std::move(indices)};
}

MiniMesh generateLeg(const tg::aabb2 &topEnd, const tg::aabb2 &bottomEnd, float length) {
    return {std::vector<tg::pos3> {
        {topEnd.min.x, 0, topEnd.min.y},
        {topEnd.min.x, 0, topEnd.max.y},
        {topEnd.max.x, 0, topEnd.min.y},
        {topEnd.max.x, 0, topEnd.max.y},

        {bottomEnd.min.x, -length, bottomEnd.min.y},
        {bottomEnd.min.x, -length, bottomEnd.max.y},
        {bottomEnd.max.x, -length, bottomEnd.min.y},
        {bottomEnd.max.x, -length, bottomEnd.max.y},
    }, std::vector<uint16_t> {
        0, 4, 5, 1, END,  // inner side
        0, 2, 6, 4, END,  // front
        2, 3, 7, 6, END,  // outer side
        1, 5, 7, 3, END,  // back
        2, 0, 1, 3, END,  // top
        4, 6, 7, 5, END,  // bottom
    }};
}

MiniMesh generateGun(const tg::size3 &size, const tg::vec3 &anchor) {
    std::vector<tg::pos3> vertices {
        {0.f, 0.f, 0.f},
        {0.f, 0.f, size.depth},
        {0.f, size.height, 0.f},
        {0.f, size.height, size.depth},
        {size.width, 0.f, 0.f},
        {size.width, 0.f, size.depth},
        {size.width, size.height, 0.f},
        {size.width, size.height, size.depth},
    };
    for (auto &v : vertices) {v -= anchor;}
    return {std::move(vertices), {
        0, 4, 5, 1, END, 3, 7, 6, 2, END,  // bottom / top
        2, 0, 1, 3, END, 7, 5, 4, 6, END,  // left / right
        4, 0, 2, 6, END, 7, 3, 1, 5, END,  // front / back
    }};
}

MiniMesh generateChest(const tg::size3 &size, const tg::size2 &waistSize, float waistOffset, const tg::pos2 &anchor, float chestHeight) {
    std::vector<tg::pos3> vertices {
        {-.5f * waistSize.width, -size.height, -waistOffset},
        {.5f * waistSize.width, -size.height, -waistOffset},
        {-.5f * waistSize.width, -size.height, -waistOffset - waistSize.height},
        {.5f * waistSize.width, -size.height, -waistOffset - waistSize.height},

        {-.5f * size.width, chestHeight, -size.depth},
        {.5f * size.width, chestHeight, -size.depth},

        {-.5f * size.width, 0, -size.depth},
        {.5f * size.width, 0, -size.depth},
        {-.5f * size.width, 0, 0},
        {.5f * size.width, 0, 0},
    };
    for (auto &v : vertices) {v.y -= anchor.x; v.z -= anchor.y;}
    return {std::move(vertices), {
        0, 1, 9, 8, END,  // back
        1, 3, 5, END, 1, 5, 9, END, 9, 5, 7, END,  // right
        5, 3, 2, 4, END, 5, 4, 6, 7, END,  // front
        0, 4, 2, END, 0, 8, 4, END, 8, 6, 4, END,  // left
        1, 0, 2, 3, END,  // bottom
        6, 8, 9, 7, END  // top
    }};
}

MiniMesh generateUpperArm(const tg::aabb2 &shoulder, const tg::aabb2 &elbow, float length) {
    return {{
        {0, shoulder.min.x, shoulder.min.y},
        {0, shoulder.min.x, shoulder.max.y},
        {0, shoulder.max.x, shoulder.min.y},
        {0, shoulder.max.x, shoulder.max.y},
        {length, elbow.min.x, elbow.min.y},
        {length, elbow.max.x, elbow.min.y},
        {length, .5f * elbow.min.x + .5f * elbow.max.x, elbow.max.y},
    }, {
        3, 1, 6, 5, 2, 0, 1, END,
        4, 6, 1, 0, 2, 5, 6, END,
    }};
}

MiniMesh generateLowerArm(const tg::aabb2 &elbow, const tg::aabb2 &wrist, float length) {
    return {{
        {length, wrist.min.x, wrist.min.y},
        {length, wrist.min.x, wrist.max.y},
        {length, wrist.max.x, wrist.min.y},
        {length, wrist.max.x, wrist.max.y},
        {0, elbow.min.x, elbow.min.y},
        {0, elbow.max.x, elbow.min.y},
        {0, .5f * elbow.min.x + .5f * elbow.max.x, elbow.max.y},
    }, {
        3, 1, 0, 2, 5, 6, 1, END,
        4, 6, 5, 2, 0, 1, 6, END,
    }};
}


MiniMesh generateHand(const tg::aabb2 &wrist, const tg::aabb2 &pads, float palmLength, float fingerLength) {
    tg::vec2 thumbVec = {1, -1};
    thumbVec *= fingerLength / tg::length(thumbVec);
    auto inch = pads.max.x - pads.min.x;
    std::vector<tg::pos3> vertices {
        {0, wrist.min.x, wrist.min.y},
        {0, wrist.min.x, wrist.max.y},
        {0, wrist.max.x, wrist.min.y},
        {0, wrist.max.x, wrist.max.y},

        {palmLength, pads.min.x, pads.min.y},
        {palmLength, pads.min.x, pads.max.y},
        {palmLength, pads.max.x, pads.min.y},
        {palmLength, pads.max.x, pads.max.y},

        {thumbVec.x, wrist.min.x, thumbVec.y + wrist.min.y},
        {thumbVec.x, wrist.min.x + inch, thumbVec.y + wrist.min.y},
        {.5f * thumbVec.x + inch, wrist.min.x, .5f * thumbVec.y + wrist.min.y},
        {.5f * thumbVec.x + inch, wrist.min.x + inch, .5f * thumbVec.y + wrist.min.y},
    };
    return {std::move(vertices), {
        0, 8, 10, 4, 5, 1, END, 2, 3, 7, 6, 11, 9, END,  // palm, back
        1, 5, 7, 3, END, 1, 3, 2, 0, END, // wrist, outer
        11, 6, 4, 10, 8, 9, END,  // between thumb and palm
        4, 6, 7, 5, END, 11, 10, 0, 2, END
    }};
}

MiniMesh generateHead(const tg::size3 &size, const tg::pos2 &anchor, const tg::pos3 &jawEnd, const tg::pos2 &chin) {
    MiniMesh res {{
        {0, size.height, -.1f * size.depth},
        {0, size.height, -.9f * size.depth},
        {0, .9f * size.height, -size.depth},
        {0, chin.x, chin.y},
        {0, .2f * size.height, -.1f * size.depth},
        {0, .5f * size.height, 0},

        {.5f * size.width, .9f * size.height, -.8f * size.depth},
        {.5f * size.width, .9f * size.height, -.2f * size.depth},
        jawEnd
    }, {
        8, 3, 6, 7, 4, 3, END, 6, 3, 2, 1, 0, 7, END,
        7, 0, 5, 4, END
    }};
    for (auto &v : res.vertices) {v.y -= anchor.x; v.z -= anchor.y;}
    res.symmetric(0);
    return res;
}

}

constexpr tg::dir3 normVec(tg::vec3 v) {return tg::normalize(v);}

Humanoid::Humanoid() {
    baseStance.feet[0] = {normVec({.15f, -1.f, .2f}), -30_deg};
    baseStance.feet[1] = {normVec({-.15f, -1.f, -.2f}), 30_deg};
    baseStance.height = .9f * hipHeight;
    baseStance.upperBodyOrient = tg::quat::from_axis_angle(tg::dir3(1, 0, 0), 10_deg);
    baseStance.hipOrient = tg::quat::from_axis_angle(tg::dir3(0, 1, 0), -20_deg);

    crouchStance.feet[0] = {normVec({.5f, -1, 0}), -60_deg};
    crouchStance.feet[1] = {normVec({0, -1, 0}), 10_deg};
    crouchStance.height = .4f * hipHeight;
    crouchStance.upperBodyOrient = tg::quat::identity;
    crouchStance.hipOrient = tg::quat::identity;

    stepStance.feet[0] = stepStance.feet[1] = {normVec({0, -1, .2f}), 0_deg};
    stepStance.height = .9f * hipHeight;
    stepStance.upperBodyOrient = tg::quat::from_axis_angle({1, 0, 0}, -30_deg);
    stepStance.hipOrient = tg::quat::identity;

    auto torsoHeight = shoulderHeight - hipHeight;
    auto walkGunOrient = Util::forwardUpOrientation({-1, 1, 0}, {0, 1, 0});
    stepGunPos[0] = stepGunPos[1] = {
        tg::pos3(.3f * shoulderDist, .5f * torsoHeight, -.25f) - walkGunOrient * tg::vec3(0, 0, gunOffset),
        walkGunOrient
    };
    stepGunPos[0].translation += tg::vec3(-.6f * shoulderDist, 0, 0);

    bodyCenter = {0, .5f * torsoHeight, 0};
    gunCenter = {0, torsoHeight, 0};

    rightHandPos = {
        {.05f, -.085f, .7f},
        Util::upForwardOrientation({1, 0, 0}, {0, 1, 0})
    };
    leftHandPos = {
        {-.12f, -.03f, .3f},
        Util::upForwardOrientation({-1, -1, 0}, {-1, 1, -1})
    };
    pumpHandPos = {
        {-.12f, -.03, .45f},
        Util::upForwardOrientation({-1, -1, 0}, {-1, 1, -1})
    };

    visual = std::make_unique<ParametricHumanoid>(*this);
}

void ParametricHumanoid::update(const Humanoid &hum) {
    vaoHip = generatePelvis(pelvicSize, hipAnchor, hum.hipJointDist).makeVAO();
    vaoGun = generateGun(gunSize, gunAnchor).makeVAO();
    vaoChest = generateChest(chestSize, waistSize, waistOffset, chestAnchor, chestHeight).makeVAO();
    vaoHead = generateHead(craniumSize, headAnchor, jawEnd, chin).makeVAO();

    auto foot = generateFoot(footSize, contactPoint, ankleJoint);
    side[0].vaoFoot = foot.makeVAO();
    foot.mirror();
    side[1].vaoFoot = foot.makeVAO();

    tg::aabb2 hipEnd = {
        {-.5f * (hum.hipJointDist - thighGap), -.5f * pelvicSize.depth},
        {.5f * (pelvicSize.width - hum.hipJointDist), .5f * pelvicSize.depth}
    };
    tg::aabb2 kneeEnd = {
        {-.5f * kneeSize.width, -.5f * kneeSize.height},
        {.5f * kneeSize.width, .5f * kneeSize.height}
    };
    auto thigh = generateLeg(hipEnd, kneeEnd, hum.hipHeight - hum.kneeHeight);
    side[0].vaoThigh = thigh.makeVAO();
    thigh.mirror();
    side[1].vaoThigh = thigh.makeVAO();

    float ankleDepth = 1.05f * footSize.depth - ankleJoint.y;
    tg::aabb2 ankleEnd = {
        {-ankleJoint.x, -.5f * ankleDepth},
        {.8f * footSize.width - ankleJoint.x, .5f * ankleDepth}
    };
    auto lowerLeg = generateLeg(
        kneeEnd, ankleEnd, hum.kneeHeight - hum.ankleHeight
    );
    side[0].vaoLowerLeg = lowerLeg.makeVAO();
    lowerLeg.mirror();
    side[1].vaoLowerLeg = lowerLeg.makeVAO();

    auto upperArm = generateUpperArm(shoulderEnd, elbowEnd, hum.elbowPos);
    side[0].vaoUpperArm = upperArm.makeVAO();
    upperArm.mirror();
    side[1].vaoUpperArm = upperArm.makeVAO();

    auto lowerArm = generateLowerArm(
        elbowEnd, wristEnd, hum.armLength -hum.elbowPos
    );
    side[0].vaoLowerArm = lowerArm.makeVAO();
    lowerArm.mirror();
    side[1].vaoLowerArm = lowerArm.makeVAO();

    auto hand = generateHand(wristEnd, padsEnd, palmLength, fingerLength);
    side[0].vaoHand = hand.makeVAO();
    hand.mirror();
    side[1].vaoHand = hand.makeVAO();
}
