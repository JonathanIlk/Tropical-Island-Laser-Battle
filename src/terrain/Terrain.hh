// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <random>

#include <typed-geometry/tg-lean.hh>
#include <polymesh/fwd.hh>
#include <polymesh/attributes.hh>
#include <glow/fwd.hh>

#include <external/SimplexNoise.h>

#include <ECS.hh>
#include <ECS/Misc.hh>
#include "TerrainMaterial.hh"

class Game;
struct MainRenderPass;
struct TerrainMaterial;

namespace Terrain {

struct Instance {
    std::unique_ptr<pm::Mesh> mesh;
    pm::vertex_attribute<tg::pos3> posAttr;

    uint32_t segmentsAmount;
    float segmentSize;
    tg::pos<2, tg::f32> center;

    float noiseScale;
    int noiseOctaves;
    float mountainHeight;
    SimplexNoise landscapeNoise;
    SimplexNoise roughnessNoise;
    float noiseOffset;
    float waterLevel = -6;
    float waterdepth = 20;
    float beachSteepness = 0.6f;

    Instance(std::mt19937 &);
    float getElevationAtPos(float xPos, float zPos) const;
    tg::pos3 getVertexPositionForSegment(int x, int z) const;
    float getIslandFalloff(float xPos, float zPos) const;
    TerrainMaterial::ID getMaterialForPosition(tg::pos3 pos) const;
};

struct Rendering {
    glow::SharedVertexArray vao;

    Rendering(const Instance &);
};

class System final : public ECS::Editor {
    ECS::ECS &mECS;
    glow::SharedProgram mShader;

public:
    System(ECS::ECS &);
    void renderMain(MainRenderPass &, const float minAlpha);
    void editorUI(ECS::entity);
    void renderingEditorUI(ECS::entity, Instance &);
};

}
