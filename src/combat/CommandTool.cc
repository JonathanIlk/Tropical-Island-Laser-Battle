// SPDX-License-Identifier: MIT
#include "CommandTool.hh"
#include <cinttypes>

#include <typed-geometry/tg-std.hh>
#include <imgui/imgui.h>

#include <MathUtil.hh>
#include <Util.hh>
#include <combat/Walking.hh>
#include <ECS/Join.hh>
#include <navmesh/NavMesh.hh>
#include <obstacles/Obstacle.hh>

using namespace Combat;

CommandTool::CommandTool(Game &game, tg::angle32 viewAngle, float innerRadius, float outerRadius) : mECS{game.mECS}, mActiveTool{game.mActiveTool} {
    mVaoMarker = game.mSharedResources.tetrahedronMarker;
    mSimpleShader = game.mSharedResources.simple;

    // it has the rough shape of a fan, but is actually a triangle strip :S
    std::vector<tg::pos3> fan;
    for (auto i : Util::IntRange(-50, 51)) {
        auto [sin, cos] = tg::sin_cos(viewAngle / 50 * i);
        fan.push_back({sin * outerRadius, 0, -cos * outerRadius});
        fan.push_back({sin * innerRadius, 0, -cos * innerRadius});
    }
    mVaoFan = glow::VertexArray::create(
        glow::ArrayBuffer::create("aPosition", fan),
        GL_TRIANGLE_STRIP
    );
}

bool CommandTool::findDestination(const tg::ray3 &ray) {
    auto ints = mECS.navMeshSys->intersect(ray);
    if (!ints) {return false;}
    auto &[navId, face, rayParam] = *ints;
    glow::info() << "endFace " << face.value;

    auto mob = mECS.mobileUnits.find(mECS.selectedEntity);
    if (mob == mECS.mobileUnits.end()) {return false;}

    auto pos = ray[rayParam];
    auto closest = mECS.obstacleSys->closest(pos);
    if (closest && closest->second <= mob->second.radius) {
        glow::warning() << "point too close to obstacle";
        return false;
    }
    mDestination = {{navId, face, pos}};
    mOrient.reset();
    return true;
}

bool CommandTool::navigate(const tg::ray3 &ray) {
    TG_ASSERT(mDestination.has_value());
    NavMesh::RouteRequest req;
    ECS::entity navId;
    std::tie(navId, req.end_face, req.end) = *mDestination;

    decltype(ECS::Snapshot::humanoids)::iterator humpos; {
        auto &humposs = mECS.simSnap->humanoids;
        humpos = humposs.find(mECS.selectedEntity);
        if (humpos == humposs.end()) {return false;}
        req.start = humpos->second.base.translation;
    }
    auto navIter = mECS.navMeshes.find(navId);
    if (navIter == mECS.navMeshes.end()) {return false;}
    auto &nav = navIter->second;
    {
        auto closest = nav.closestPoint(req.start);
        if (!closest) {return false;}
        auto [faceId, dist] = *closest;
        req.start_face = faceId;
        glow::info() << "faceId " << faceId.value;
    }
    auto humJoin = ECS::Join(mECS.mobileUnits, mECS.humanoids);
    auto humIter = humJoin.find(mECS.selectedEntity);
    if (humIter == humJoin.end()) {return false;}
    auto [mob, hum, id] = *humIter;

    if (!mOrient) {return false;}
    auto mat = tg::mat3(*mOrient);

    if (!mob.planRoute(*navIter, req, mECS)) {
        mDestination.reset();
        return false;
    }
    MovementContext {mECS, hum, mob, nav}.planWalk(
        humpos->second, tg::dir3(mat[1]), -mat[2]
    );
    mECS.selectedEntity = ECS::INVALID;
    std::unique_ptr<Game::Tool> dummy;
    std::swap(mActiveTool, dummy);
    return true;
}

bool CommandTool::onClick(const tg::ray3 &ray) {
    if (!mDestination) {
        return findDestination(ray);
    } else {
        return navigate(ray);
    }
}

void CommandTool::processInput(const glow::input::InputState &, const tg::ray3 &ray) {
    if (!mDestination) {return;}
    auto iter = mECS.staticRigids.find(std::get<0>(*mDestination));
    if (iter == mECS.staticRigids.end()) {return;}
    auto up = iter->second.rotation * tg::dir3(0, 1, 0);

    auto dest = std::get<2>(*mDestination);
    auto offset = tg::dot(dest, up);
    auto cos = tg::dot(up, ray.dir);
    if (!cos) {
        mOrient.reset();
        return;
    }
    auto param = (offset - tg::dot(ray.origin, up)) / cos;
    auto dirVec = ray[param] - dest;
    auto length = tg::length(dirVec);
    if (param < 0.f || length == 0.f) {
        mOrient.reset();
        return;
    }
    mOrient = {Util::upForwardOrientation(up, dirVec)};
}

void CommandTool::renderMain(MainRenderPass &pass) {
    auto ent = mECS.selectedEntity;
    auto iter = pass.snap->humanoids.find(ent);
    if (iter == pass.snap->humanoids.end()) {
        glow::warning() << "CommandTool without humanoid";
        return;
    }
    auto &humpos = iter->second;
    mSimpleShader->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto sh = mSimpleShader->use();
    pass.applyCommons(sh);
    sh["uPickID"] = ECS::INVALID;
    sh["uAlbedo"] = tg::color3(.2, .2, .2);
    sh["uARM"] = tg::vec3(1, .95, 0);
    sh["uEmission"] = tg::color3(.8f, 0, 0);
    auto pos = humpos.base;
    // the markers are pure UI features, so they are animated from wall time
    auto angle = tg::angle32::from_radians(std::fmod(
        5 * pass.wallTime, 360_deg .radians()
    ));
    auto markerOrient = pos.rotation * tg::quat::from_axis_angle({0, 1, 0}, angle);
    sh["uModel"] = Util::transformMat(
        pos.translation + pos.rotation * tg::vec3(0, 2.f, 0),
        markerOrient,
        tg::size3(.5f)
    );
    mVaoMarker->bind().draw();
    if (mDestination) {
        sh["uEmission"] = tg::color3(0, .8f, 0);
        auto dest = std::get<2>(*mDestination);
        sh["uModel"] = Util::transformMat(dest, markerOrient);
        mVaoMarker->bind().draw();
        if (mOrient) {
            sh["uModel"] = Util::transformMat(
                dest + *mOrient * tg::vec3(0, .5f, 0), *mOrient
            );
            mVaoFan->bind().draw();
        }
    }
}

void CommandTool::updateUI() {
    auto ent = mECS.selectedEntity;
    ImGui::Text("Moving entity %" PRIu32, ent);
    if (mDestination) {
        ImGui::TextUnformatted("Click on ground to select view target");
    } else {
        ImGui::TextUnformatted("Click on ground to select destination");
    }
    auto mobIter = mECS.mobileUnits.find(ent);
    if (mobIter == mECS.mobileUnits.end()) {
        ImGui::TextUnformatted("Selected entity is not a MobileUnit");
        return;
    }
    auto &mob = mobIter->second;
    ImGui::SliderFloat("Acceleration", &mob.acceleration, 0.001f, 20.f, "%.3f", 2.f);
    ImGui::SliderFloat("Cruise Speed", &mob.cruiseSpeed, 0.001f, 30.f);
    auto humIter = mECS.humanoids.find(ent);
    if (humIter != mECS.humanoids.end()) {
        auto &hum = humIter->second;
        ImGui::SliderFloat("Stride length", &hum.strideLength, .01f, 1.f);
        ImGui::SliderFloat("Steps / second", &hum.stepsPerSecond, .5f, 10.f);
    }
}
