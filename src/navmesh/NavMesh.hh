// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>
#include <polymesh/Mesh.hh>
#include <glow/fwd.hh>

#include <ECS.hh>
#include <ECS/Misc.hh>
#include <obstacles/Collision.hh>

namespace Terrain {
    struct Instance;
}

class Game;

namespace NavMesh {

struct FaceInfo {
    tg::aabb3 aabb;
    pm::face_index idx;

    tg::aabb3 getAABB() const {return aabb;}
};

struct RouteRequest {
    tg::pos3 start, end;
    pm::face_index start_face, end_face;
};

using Route = std::vector<std::pair<pm::halfedge_index, float>>;

struct Instance {
    std::unique_ptr<pm::Mesh> mesh = std::make_unique<pm::Mesh>();
    pm::vertex_attribute<tg::pos3> localPos{*mesh};

    // positions and index used in navigation are in world space, not local space;
    // navigation is really terrible if you have to keep converting coordinate spaces
    pm::vertex_attribute<tg::pos3> worldPos{*mesh};
    ECS::RTree<FaceInfo> faceTree;

    Instance(const ECS::Rigid &wo, const Terrain::Instance &terrain);

    tg::pos3 edgeLerp(const pm::edge_handle &edge, float param) const;
    Route navigate(const RouteRequest &req, uint32_t nsteps, Obstacle::Collider &collider);
    std::optional<std::pair<pm::face_index, float>> closestPoint(tg::pos3 pos) const;
    std::optional<float> intersectionTest(pm::face_handle f, const tg::ray3 &ray) const;
    std::optional<std::pair<pm::face_index, float>> intersect(const tg::ray3 &ray) const;
    /// CAUTION: result NOT normalized
    tg::vec3 faceNormal(pm::face_index f) const;
};

class System final : public ECS::Editor {
    ECS::ECS &mECS;

public:
    System(ECS::ECS &ecs) : mECS{ecs} {}
    void editorUI(ECS::entity);
    std::optional<std::tuple<ECS::entity, pm::face_index, float>> intersect(const tg::ray3 &ray);
};

}
