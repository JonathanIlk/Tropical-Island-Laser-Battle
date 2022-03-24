// SPDX-License-Identifier: MIT
#include "Misc.hh"
#include <glow/common/log.hh>

#include <ECS.hh>
#include <MathUtil.hh>
#include <combat/Combat.hh>
#include <demo/Demo.hh>
#include <effects/Effects.hh>
#include <navmesh/NavMesh.hh>
#include <obstacles/Obstacle.hh>
#include <obstacles/WorldFluff.hh>
#include <rendering/MeshViz.hh>
#include <startsequence/StartSequence.hh>
#include <terrain/SkyBox.hh>
#include <terrain/Terrain.hh>
#include <terrain/Water.hh>
#include <ui/SpriteRenderer.hh>
#include <environment/Parrot.hh>

#include "SimpleMesh.hh"

ECS::entity ECS::ECS::newEntity() {
    if (freeEntities.empty()) {
        glow::info() << "allocating new entity " << nextEntity;
        return nextEntity++;
    }
    auto res = freeEntities.back();
    glow::info() << "recycling entity ID " << res;
    freeEntities.pop_back();
    return res;
}
void ECS::ECS::deleteEntity(entity id) {
    editables.erase(id);

    humanoids.erase(id);
    mobileUnits.erase(id);
    demoAnim.erase(id);
    staticRigids.erase(id);
    instancedRigids.erase(id);
    vizMeshes.erase(id);
    navMeshes.erase(id);
    obstacles.erase(id);
    simpleMeshes.erase(id);
    scatterLasers.erase(id);
    skyBoxes.erase(id);
    startSequenceObjects.erase(id);
    terrains.erase(id);
    terrainRenderings.erase(id);
    waters.erase(id);
    worldFluffs.erase(id);
    riggedMeshes.erase(id);
    riggedRigids.erase(id);
    parrots.erase(id);

    freeEntities.push_back(id);
}

tg::mat4x3 ECS::Rigid::transform_mat() const {
    return Util::transformMat(translation, rotation);
}
ECS::Rigid ECS::Rigid::operator~() const {
    auto rot = tg::conjugate(rotation);
    return {tg::pos3(rot * -translation), rot};
}
ECS::Rigid ECS::Rigid::chain(const Rigid &other) const {
    auto mat = transform_mat();
    return {
        tg::pos3(mat * tg::vec4(other.translation, 1.f)),
        tg::normalize(rotation * other.rotation)
    };
}
ECS::Rigid ECS::Rigid::interpolate(const Rigid &other, float param) const {
    return {
        tg::lerp(translation, other.translation, param),
        tg::slerp(rotation, other.rotation, param)
    };
}
tg::vec3 ECS::operator*(const Rigid &a, const tg::vec3 &b) {
    return a.rotation * b;
}
tg::dir3 ECS::operator*(const Rigid &a, const tg::dir3 &b) {
    return a.rotation * b;
}
tg::pos3 ECS::operator*(const Rigid &a, const tg::pos3 &b) {
    return a.translation + a.rotation * b;
}

void ECS::ECS::init(Game &game) {
    combatSys = std::make_unique<Combat::System>(game);
    demoSys = std::make_unique<Demo::System>(*this);
    effectsSys = std::make_unique<Effects::System>(game);
    navMeshSys = std::make_unique<NavMesh::System>(*this);
    meshVizSys = std::make_unique<MeshViz::System>(*this);
    obstacleSys = std::make_unique<Obstacle::System>(game);
    terrainSys = std::make_unique<Terrain::System>(*this);
    waterSys = std::make_unique<Water::System>(*this);
    skyBoxSys = std::make_unique<SkyBox::System>(*this);
    worldFluffSys = std::make_unique<WorldFluff::System>(game);
    startSequenceSys = std::make_unique<StartSequence::System>(game);
    spriteRendererSys = std::make_unique<SpriteRenderer::System>(game);
    riggedMeshSys = std::make_unique<RiggedMesh::System>(game);
    parrotSys = std::make_unique<Parrot::System>(game);
}

ECS::ECS::~ECS() {}

void ECS::ECS::extrapolateUpdate(Snapshot &prev, Snapshot &next) {
    next.humRender.clear();
    // put here: extrapolate all interacting objects (or those whose position
    // is computed by integration), then compute their interactions
    combatSys->extrapolate(prev, next);

    combatSys->update(prev, next);
}

void ECS::ECS::fixedUpdate() {
    parrotSys->behaviorUpdate();
}

void ECS::ECS::cleanup(double time) {
    effectsSys->cleanup(time);
}

void ECS::ECS::extrapolateRender(Snapshot &upd, Snapshot &render) {
    render.humRender.clear();
    // extrapolate ALL objects
    render.rigids = staticRigids;
    render.riggedRigids = riggedRigids;
    demoSys->extrapolate(render);
    combatSys->extrapolate(upd, render);
    combatSys->prepareRender(render);
}

void ECS::ECS::renderShadow(MainRenderPass &pass) {
    demoSys->renderMain(pass);
    obstacleSys->renderMain(pass);
    worldFluffSys->renderMain(pass);
    terrainSys->renderMain(pass, 1.0f);
}

void ECS::ECS::renderSSAO(MainRenderPass &pass) {
    combatSys->renderMain(pass);
    demoSys->renderMain(pass);
    obstacleSys->renderMain(pass);
    worldFluffSys->renderMain(pass);
    startSequenceSys->renderMain(pass);
    terrainSys->renderMain(pass, 1.f);
}

void ECS::ECS::renderReflectRefract(MainRenderPass &pass) {
    combatSys->renderMain(pass);
    demoSys->renderMain(pass);
    obstacleSys->renderMain(pass);
    skyBoxSys->renderMain(pass);
    terrainSys->renderMain(pass, 0.0f);
}

void ECS::ECS::renderMain(MainRenderPass &pass) {
    combatSys->renderMain(pass);
    demoSys->renderMain(pass);
    meshVizSys->renderMain(pass);
    obstacleSys->renderMain(pass);
    terrainSys->renderMain(pass, 1.0f);
    waterSys->renderMain(pass);
    skyBoxSys->renderMain(pass);
    worldFluffSys->renderMain(pass);
    startSequenceSys->renderMain(pass);
    riggedMeshSys->renderMain(pass);
}

void ECS::ECS::renderTransparent(MainRenderPass &pass) {
    effectsSys->renderMain(pass);
    startSequenceSys->renderTransparent(pass);
}

void ECS::ECS::renderUi() {
    spriteRendererSys->render();
}
