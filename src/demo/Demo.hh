// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>
#include <glow/fwd.hh>

#include <fwd.hh>

namespace Demo {

struct Animation {
    tg::pos3 basePosition;
    float bounceSpeed = 0.f;
    tg::vec3 bounceVec = tg::vec3::zero;
    tg::vec3 angularVelocity = tg::vec3::zero;
};

class System final {
private:
    ECS::ECS &mECS;

    glow::SharedTexture2D mTexAlbedo;
    glow::SharedTexture2D mTexNormal;
    glow::SharedTexture2D mTexARM;

    glow::SharedVertexArray mVaoCube;
    glow::SharedVertexArray mVaoSphere;

    glow::SharedProgram mShaderObject;
    glow::SharedProgram mShaderObjectSimple;

public:
    System(ECS::ECS &);
    void addScene(int seed, tg::pos3 basePos, tg::quat baseRot);
    void renderMain(MainRenderPass &);
    void extrapolate(ECS::Snapshot &next);
};

}
