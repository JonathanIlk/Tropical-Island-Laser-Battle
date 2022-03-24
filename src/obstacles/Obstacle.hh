// SPDX-License-Identifier: MIT
#pragma once
#include <optional>

#include <polymesh/Mesh.hh>
#include <glow/objects.hh>

#include <ECS.hh>
#include <ECS/Misc.hh>
#include <animation/rigged/RiggedMesh.hh>
#include <rendering/InstancedRenderer.hh>
#include <terrain/TerrainMaterial.hh>
#include <util/SparseDiscreteDistribution.hh>
#include "../../extern/typed-geometry/src/typed-geometry/feature/quat.hh"
#include "../../extern/typed-geometry/src/typed-geometry/types/pos.hh"
#include "Collision.hh"

namespace Obstacle
{

struct Type {
    int id;
    InstancedRenderer::VaoInfo &vaoInfo;
    std::unique_ptr<CollisionMesh> collisionMesh;
    bool highCover;

    Type(InstancedRenderer::VaoInfo &vaoInfo) : vaoInfo{vaoInfo} {}
    Type(Type &&) = default;
};

class System final : public ECS::Editor {
    ECS::ECS &mECS;
    SharedResources& mSharedResources;

    InstancedRenderer mInstancedRenderer;

    float obstacleDensity = 0.1f;
    float parrotDensity = 0.45;

    std::vector<Type> mTypes;
    std::array<SparseDiscreteDistribution<size_t, float>, TerrainMaterial::NumMaterials> mTypesForTerrain;

    std::vector<tg::pos3> randomlySelectedObstaclePositions(Terrain::Instance& terr, std::mt19937& engine) const;
    void initObstacleCollider(CollisionMesh &collider, const char *meshName) const;

    void spawnParrot(const ECS::Rigid& wo, const tg::quat& randomRotation, const tg::pos3& worldPos, std::mt19937& rng);

public:
    std::map<glow::SharedVertexArray, CollisionMesh> obstacleColliders;

    System(Game&);

    void renderMain(MainRenderPass&);
    void editorUI(ECS::entity);

    void spawnObstacles(ECS::Rigid &wo, Terrain::Instance &terr, std::mt19937& rng);
    using QueryResult = std::optional<std::pair<ECS::entity, float>>;
    QueryResult rayCast(const tg::ray3 &ray) const;
    QueryResult closest(const tg::pos3 &pos) const;
};
}
