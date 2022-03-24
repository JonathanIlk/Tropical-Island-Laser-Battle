// SPDX-License-Identifier: MIT
#pragma once
#include <memory>

#include <typed-geometry/tg-lean.hh>
#include <glow/fwd.hh>

#include <fwd.hh>
#include <ECS.hh>
#include <ECS/Misc.hh>
#include <rendering/MainRenderPass.hh>

namespace SkyBox
{
struct Instance
{
    tg::mat4 skyBoxTransformation;
    tg::color3 sandColor;

    explicit Instance(const Terrain::Instance &);
};

class System final
{
    glow::SharedVertexArray mVao;

    ECS::ComponentMap<Instance> & mSkyBoxes;
    ECS::ComponentMap<Terrain::Instance> &mTerrains;

    glow::SharedProgram mShaderSkyBox;
    glow::SharedTexture2D mLowPolyNormalsTex;
    glow::SharedTexture2D mLowPolyNormals2Tex;


public:
    explicit System(ECS::ECS &ecs);
    void renderMain(MainRenderPass &pass);
};

}
