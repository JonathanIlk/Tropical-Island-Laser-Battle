// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "fwd.hh"
#include "rtree/RTree.hh"
#include "rtree/TGDomain.hh"

namespace ECS {

struct Rigid;
class Editor;

using entity = std::uint32_t;
static constexpr entity INVALID = -1;

template<typename T>
using ComponentMap = std::map<entity, T>;
template<typename T>
using RTree = RTree<T, TGDomain<3, float>>;

/// collects the positions of objects at a particular point in time.
/// By using the same output struct, the extrapolation code can be shared
/// between simulation and rendering
struct Snapshot {
    // absolute time values are stored as doubles because floats (typically IEEE
    // binary32) only have 24 bits of effective precision, meaning they can keep
    // millisecond precision for only about 4h40min (twice that if you make use of
    // negative values, too), while doubles will last for thousands of years, which
    // should be enough for our needs. Most time deltas we deal with will be
    // on the order of seconds, so single-precision should be fine for the
    // purpose of interpolating animations and such.
    double worldTime = 0.;

    ComponentMap<Rigid> rigids;
    ComponentMap<Rigid> riggedRigids;
    ComponentMap<Combat::HumanoidPos> humanoids;

    std::vector<Combat::HumanoidRenderInfo> humRender;
};

struct ECS {
    Snapshot *simSnap;

    // === Entity management
    std::vector<entity> freeEntities;
    entity nextEntity = 0;
    entity newEntity();
    entity selectedEntity = INVALID;
    /// CAUTION: beware iterator/reference invalidation when using this method
    void deleteEntity(entity id);

    // === Systems
    // define them here, such that Components can hold non-owning references
    // to their members without lifetime problems
    std::unique_ptr<Combat::System> combatSys;
    std::unique_ptr<Demo::System> demoSys;
    std::unique_ptr<Effects::System> effectsSys;
    std::unique_ptr<MeshViz::System> meshVizSys;
    std::unique_ptr<NavMesh::System> navMeshSys;
    std::unique_ptr<Obstacle::System> obstacleSys;
    std::unique_ptr<Terrain::System> terrainSys;
    std::unique_ptr<Water::System> waterSys;
    std::unique_ptr<SkyBox::System> skyBoxSys;
    std::unique_ptr<WorldFluff::System> worldFluffSys;
    std::unique_ptr<StartSequence::System> startSequenceSys;
    std::unique_ptr<SpriteRenderer::System> spriteRendererSys;
    std::unique_ptr<RiggedMesh::System> riggedMeshSys;
    std::unique_ptr<Parrot::System> parrotSys;

    // === Components
    ComponentMap<Editor *> editables;

    // sorted by qualified name of component type
    ComponentMap<Combat::Humanoid> humanoids;
    ComponentMap<Combat::MobileUnit> mobileUnits;
    ComponentMap<Demo::Animation> demoAnim;
    ComponentMap<Effects::ScatterLaser> scatterLasers;
    ComponentMap<Rigid> instancedRigids;
    ComponentMap<Rigid> staticRigids;
    ComponentMap<MeshViz::Instance> vizMeshes;
    ComponentMap<NavMesh::Instance> navMeshes;
    ComponentMap<Obstacle::Type &> obstacles;
    ComponentMap<SimpleMesh> simpleMeshes;
    ComponentMap<Terrain::Instance> terrains;
    ComponentMap<Terrain::Rendering> terrainRenderings;
    ComponentMap<Water::Instance> waters;
    ComponentMap<SkyBox::Instance> skyBoxes;
    ComponentMap<WorldFluff::Type &> worldFluffs;
    ComponentMap<StartSequence::Instance> startSequenceObjects;
    ComponentMap<Rigid> riggedRigids;
    ComponentMap<RiggedMesh::Instance> riggedMeshes;
    ComponentMap<Parrot::Instance> parrots;

    ComponentMap<SpriteRenderer::Instance> sprites;

    RTree<Obstacle::Obstruction> obstructions;

    void init(Game &game);
    ~ECS();

    void extrapolateUpdate(Snapshot &prev, Snapshot &next);
    void extrapolateRender(Snapshot &upd, Snapshot &render);
    void fixedUpdate();

    void cleanup(double time);

    void renderShadow(MainRenderPass &pass);
    void renderSSAO(MainRenderPass &pass);
    void renderReflectRefract(MainRenderPass &pass);
    void renderMain(MainRenderPass &pass);
    void renderTransparent(MainRenderPass &);
    void renderUi();
};

}
