// SPDX-License-Identifier: MIT
#include "PathfinderTool.hh"
#include <cstdio>

#include <typed-geometry/tg-std.hh>
#include <typed-geometry/types/objects/ray.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/VertexArray.hh>
#include <imgui/imgui.h>

#include <MathUtil.hh>
#include <ECS/Misc.hh>
#include <ECS/Join.hh>
#include <navmesh/NavMesh.hh>
#include <obstacles/Obstacle.hh>
#include <rendering/MainRenderPass.hh>

NavMesh::PathfinderTool::PathfinderTool(Game &game) : mECS{game.mECS} {
    mVaoMarker = game.mSharedResources.tetrahedronMarker;
    mShader = game.mSharedResources.simple;
    // mABPath is filled whenever a new path visualization is created
    mABPath = glow::ArrayBuffer::create();
    mABPath->defineAttribute<tg::pos3>("aPosition");
    mVaoPath = glow::VertexArray::create(
        mABPath,
        GL_TRIANGLE_STRIP
    );
}

static tg::dir3 halfedgeNormal(const pm::vertex_attribute<tg::pos3> &pos, pm::halfedge_handle he) {
    auto prev = pos[he.vertex_from()], cur = pos[he.vertex_to()], next = pos[he.next().vertex_to()];
    return tg::normalize(tg::cross(next - cur, prev - cur));
}

bool NavMesh::PathfinderTool::onClick(const tg::ray3 &ray) {
    mPoints[mCurPoint] = std::nullopt;
    auto ints = mECS.navMeshSys->intersect(ray);
    if (ints) {
        auto [ent, idx, depth] = *ints;
        mPoints[mCurPoint] = {ray[depth], ent, idx};
    } else {return false;}
    if (mPoints[mCurPoint] && !mPoints[(mCurPoint + 1) % mPoints.size()]) {
        mCurPoint = (mCurPoint + 1) % mPoints.size();
    }
    return updatePath();
}

bool NavMesh::PathfinderTool::updatePath() {
    if (!mPoints[0].has_value() || !mPoints[1].has_value()) {return true;}
    if (mPoints[0]->navmesh != mPoints[1]->navmesh) {return false;}

    auto join = ECS::Join(mECS.navMeshes, mECS.staticRigids);
    auto navIter = join.find(mPoints[0]->navmesh);
    if (navIter == join.end()) {
        // NavMesh has disappeared, make tabula rasa
        mPoints = {std::nullopt, std::nullopt};
        glow::warning() << "navmesh disappeared";
        return false;
    }
    auto [nav, rigid, id] = *navIter;

    for (size_t i = 0; i < mPoints.size(); ++i) {
        if (!nav.mesh->index_exists(mPoints[i]->face)) {
            glow::warning() << "face index invalid";
            mPoints[i] = std::nullopt;
            mCurPoint = i;
            return false;
        }
        auto pos = mPoints[i]->pos;
        bool found = false;
        nav.faceTree.visit([&, pos] (const tg::aabb3 &a, decltype(nav.faceTree)::level_t) {
            return tg::contains(a, pos);
        }, [&, pos] (const NavMesh::FaceInfo &a) {
            if (!tg::contains(a.aabb, pos)) {return true;}
            found = true;
            return false;
        });
        if (!found) {
            glow::warning() << "position invalid";
            mPoints[i] = std::nullopt;
            mCurPoint = i;
            return false;
        }
        auto closest = mECS.obstacleSys->closest(pos);
        if (closest && closest->second <= mRadius) {
            glow::warning() << "point too close to obstacle";
            mPoints[i] = std::nullopt;
            mCurPoint = i;
            return false;
        }
    }

    if (mPoints[0]->face == mPoints[1]->face) {
        glow::warning() << "points in the same face";
        return false;  // TODO: path in the same face
    }
    NavMesh::RouteRequest req = {
        mPoints[0]->pos, mPoints[1]->pos,
        mPoints[0]->face, mPoints[1]->face,
    };
    auto up = rigid.rotation * tg::dir3(0, 1, 0);
    Obstacle::Collider collider(mECS, up * mHeight, mRadius);
    auto res = nav.navigate(req, mNSteps, collider);
    glow::info() << collider.query.nFaceChecks << " face checks, " << collider.query.nAABBChecks << " AABB checks";
    std::vector<tg::pos3> pathVizVerts;
    pathVizVerts.reserve(res.size() * 2 + 2 + 4 * collider.query.rejected.size());
    pathVizVerts.push_back(mPoints[0]->pos);
    for (auto &pair : res) {
        auto &[heIdx, pos] = pair;
        auto he = nav.mesh->handle_of(heIdx);
        auto a = nav.worldPos[he.vertex_from()];
        auto b = nav.worldPos[he.vertex_to()];
        auto normal = tg::normalize_safe(
            halfedgeNormal(nav.worldPos, he)
            + halfedgeNormal(nav.worldPos, he.prev())
            + halfedgeNormal(nav.worldPos, he.opposite())
            + halfedgeNormal(nav.worldPos, he.opposite().prev())
        );
        auto midpoint = tg::lerp(a, b, pos) + .25f * normal;
        auto edgeDir = tg::normalize(b - a);
        pathVizVerts.push_back(midpoint - .1f * edgeDir);
        pathVizVerts.push_back(midpoint + .1f * edgeDir);
    }
    pathVizVerts.push_back(mPoints[1]->pos);
    mPathEnd = pathVizVerts.size();

    for (auto &seg : collider.query.rejected) {
        auto vec = seg.pos1 - seg.pos0;
        auto left = tg::cross(up, vec);
        left *= .05f / tg::length(left);
        for (auto v : {
            seg.pos0 + left, seg.pos0 - left, seg.pos1 + left, seg.pos1 - left
        }) {
            pathVizVerts.push_back(v);
        }
    }
    mVaoEnd = pathVizVerts.size();

    glow::info() << "uploading " << pathVizVerts.size() << " vertices";
    mABPath->bind().setData(pathVizVerts);
    return true;
}

void NavMesh::PathfinderTool::updateUI() {
    for (size_t i = 0; i < mPoints.size(); ++i) {
        char text[128];
        snprintf(text, sizeof(text), "Point %zu: ", i);
        if (ImGui::RadioButton(text, mCurPoint == i)) {
            mCurPoint = i;
        }
        ImGui::SameLine();
        if (mPoints[i].has_value()) {
            auto &sel = mPoints[i]->pos;
            ImGui::Text("%.2f %.2f %.2f", sel.x, sel.y, sel.z);
        } else {
            ImGui::TextUnformatted("not selected");
        }
    }
    bool update = false;
    if ((update |= ImGui::InputInt("#crossings per edge", &mNSteps, 1, 100))) {
        mNSteps = std::clamp(mNSteps, 1, 100);
    }
    update |= ImGui::InputFloat("Path width", &mRadius, 0.f, 10.f);
    update |= ImGui::InputFloat("Unit height", &mHeight, 0.f, 10.f);
    if (update) {updatePath();}
}

void NavMesh::PathfinderTool::renderMain(MainRenderPass &pass) {
    mShader->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto sh = mShader->use();
    pass.applyCommons(sh);
    sh["uPickID"] = ECS::INVALID;
    sh["uAlbedo"] = tg::vec3(.2, .2, .2);
    sh["uARM"] = tg::vec3(1, .95, 0);

    std::array<uint32_t, 2> colors = {0x00cc00ff, 0xcc0000ff};
    bool drawPath = true;
    {
        auto va = mVaoMarker->bind();
        for (size_t i = 0; i < mPoints.size(); ++i) {
            if (!mPoints[i].has_value()) {drawPath = false; continue;}
            // the markers are pure UI features, so they are animated from wall time
            auto angle = tg::angle32::from_radians(std::fmod(5 * pass.wallTime, 360_deg .radians()));
            sh["uModel"] = Util::transformMat(mPoints[i]->pos, tg::quat::from_axis_angle({0, 1, 0}, angle));
            sh["uEmission"] = tg::vec3(Util::unpackSRGBA(colors.at(i)));
            va.draw();
        }
    }
    if (drawPath) {
        sh["uModel"] = tg::mat4x3::identity;
        sh["uEmission"] = tg::color3(Util::unpackSRGBA(colors[0]));
        auto vao = mVaoPath->bind();
        vao.drawRange(0, mPathEnd);
        sh["uEmission"] = tg::color3(Util::unpackSRGBA(colors[1]));
        for (size_t i = mPathEnd; i < mVaoEnd; i += 4) {
            vao.drawRange(i, i + 4);
        }
    }
}
