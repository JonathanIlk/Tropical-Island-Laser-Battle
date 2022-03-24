// SPDX-License-Identifier: MIT
#pragma once
#include <polymesh/fwd.hh>
#include <glow/fwd.hh>

#include <ECS.hh>
#include <ECS/Misc.hh>
#include <rendering/InstancedRenderer.hh>
#include <terrain/TerrainMaterial.hh>
#include <util/SparseDiscreteDistribution.hh>

class Game;

namespace WorldFluff
{

struct Type {
    InstancedRenderer::VaoInfo &vaoInfo;

    Type(InstancedRenderer::VaoInfo &vaoInfo) : vaoInfo{vaoInfo} {}
};

class System final {
    ECS::ECS &mECS;

    InstancedRenderer mInstancedRenderer;

    float fluffDensity = 0.20;

    std::vector<Type> mTypes;
    std::array<SparseDiscreteDistribution<size_t, float>, TerrainMaterial::NumMaterials> mTypesForTerrain;

    std::vector<polymesh::face_handle> randomlySelectFluffPositions(Terrain::Instance& terr, std::mt19937& rng) const;

public:
    System(Game&);
    void renderMain(MainRenderPass&);
    void spawnFluff(ECS::Rigid &wo, Terrain::Instance &terr, std::mt19937& rng);
};
}
