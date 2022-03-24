// SPDX-License-Identifier: MIT
#include "SpawnTool.hh"
#include <imgui/imgui.h>

#include "Walking.hh"
#include <ECS/Join.hh>
#include <navmesh/NavMesh.hh>
#include <obstacles/Obstacle.hh>

using namespace Combat;

bool SpawnTool::onClick(const tg::ray3 &ray) {
    return spawnUnit(ray).has_value();
}

void SpawnTool::updateUI() {
    ImGui::TextUnformatted("Click on a navigable surface to spawn a humanoid unit");
}
std::optional<ECS::entity> SpawnTool::spawnUnit(const tg::ray3& ray) {
    auto ints = mECS.navMeshSys->intersect(ray);
    if (!ints) {return {};}
    auto [navId, face, param] = *ints;

    auto radius = .5f;
    auto pos = ray[param];
    auto closest = mECS.obstacleSys->closest(pos);
    if (closest && closest->second <= radius) {
        glow::warning() << "Position too close to an obstacle";
        return {};
    }

    auto join = ECS::Join(mECS.staticRigids, mECS.navMeshes);
    auto iter = join.find(navId);
    if (iter == join.end()) {return {};}
    auto [rigid, nav, navId_] = *iter;

    auto ent = mECS.newEntity();
    mECS.editables.emplace(ent, &*mECS.combatSys);
    auto &hum = mECS.humanoids.emplace(ent, Humanoid()).first->second;
    auto &mob = mECS.mobileUnits.emplace(ent, MobileUnit()).first->second;
    mob.cruiseSpeed = 10.f;
    mob.acceleration = 7.f;
    mob.radius = radius;
    mob.heightVector = rigid.rotation * tg::vec3(0, 1.8f, 0);
    mECS.simSnap->humanoids.emplace(ent, MovementContext {mECS, hum, mob, nav}.restPos({pos, rigid.rotation})
    );
    return ent;
}
