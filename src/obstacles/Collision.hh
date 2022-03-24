// SPDX-License-Identifier: MIT
#pragma once
#include <polymesh/Mesh.hh>

#include <ECS.hh>

namespace Obstacle {

struct IndexedFace {
    tg::aabb3 aabb;
    pm::face_index idx;

    tg::aabb3 getAABB() const {return aabb;}
};

struct MeshWithNormals {
    pm::Mesh mesh;
    pm::vertex_attribute<tg::pos3> position = mesh.vertices().make_attribute<tg::pos3>();
    pm::face_attribute<tg::halfspace3> normals = mesh.faces().make_attribute<tg::halfspace3>();

    bool intersectsAABB(const tg::aabb3 &) const;
    bool faceIntersectsSegment(pm::face_handle face, const tg::segment3 &, float epsilon = 0.001f) const;
};

struct CollisionQuery final : public MeshWithNormals {
    tg::segment3 seg;
    size_t nObstructionTests = 0, nFaceChecks = 0, nAABBChecks = 0;
    std::vector<tg::segment3> rejected;
};

struct CollisionMesh final : public MeshWithNormals {
    ECS::RTree<tg::pos3> vertexTree;
    ECS::RTree<tg::segment3> edgeTree;
    ECS::RTree<IndexedFace> faceTree;

    bool collides(CollisionQuery &) const;
};

struct Obstruction {
    tg::aabb3 aabb;
    ECS::entity id;

    tg::aabb3 getAABB() const {return aabb;}
};

class Collider {
    ECS::ECS &mECS;
    tg::vec3 mHeight;
    float mRadius;

    std::vector<std::pair<ECS::Rigid, const CollisionMesh &>> mObjects;


public:
    CollisionQuery query;

    Collider(ECS::ECS &ecs, tg::vec3 height = {0, 1.5f, 0}, float radius = 1.f);

    void collectObjects(const tg::aabb3 &);
    bool segmentObstructed(const tg::segment3 &);
};

}
