// SPDX-License-Identifier: MIT
#pragma once
#include <memory>

#include <typed-geometry/tg-lean.hh>
#include <glow/fwd.hh>

#include <fwd.hh>
#include <ECS.hh>
#include <ECS/Misc.hh>
#include <rendering/MainRenderPass.hh>

namespace Water
{
struct Instance
{
    float animationSpeed = .2f;
    float waveHeight = .2f;
    glow::SharedVertexArray vao;

    glow::SharedFramebuffer fbRefract;
    glow::SharedTextureRectangle refractColor;
    glow::SharedTextureRectangle refractDepth;

    glow::SharedFramebuffer fbReflect;
    glow::SharedTextureRectangle reflectColor;
    glow::SharedTextureRectangle reflectDepth;

    Instance(const Terrain::Instance &, tg::isize2 fbSize);
    void clearFramebuffers(tg::color3 bgColor);
};

class System final
{
    ECS::ComponentMap<Instance> &mWaters;
    ECS::ComponentMap<Terrain::Instance> &mTerrains;
    glow::SharedProgram mShaderWater;

public:
    System(ECS::ECS &ecs);
    void resize(tg::isize2 screenSize);
    void renderMain(MainRenderPass &pass);
    void addInstance(ECS::entity, const Terrain::Instance &);

    static tg::mat4 calculateReflectionMatrix(tg::vec4 waterLevel);
};

}
