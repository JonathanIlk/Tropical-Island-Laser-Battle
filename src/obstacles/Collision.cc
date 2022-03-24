// SPDX-License-Identifier: MIT
#include "Obstacle.hh"
#include <typed-geometry/tg.hh>

#include <Util.hh>
#include <ECS/Join.hh>

using namespace Obstacle;

void Collider::collectObjects(const tg::aabb3 &aabb_) {
    mObjects.clear();
    tg::aabb3 aabb = TGDomain<3, float>().union_(
        aabb_, {aabb_.min + mHeight, aabb_.max + mHeight}
    );
    aabb.min -= mRadius;
    aabb.max += mRadius;
    mECS.obstructions.visit([aabb] (const tg::aabb3 &a, int) {
        return tg::intersects(a, aabb);
    }, [&] (const Obstruction &a) {
        if (!tg::intersects(a.aabb, aabb)) {return true;}
        auto join = ECS::Join(mECS.obstacles, mECS.instancedRigids);
        auto iter = join.find(a.id);
        if (iter == join.end()) {return true;}
        auto [obstacleType, rigid, id] = *iter;
        mObjects.emplace_back(rigid, *obstacleType.collisionMesh);
        return true;
    });
}

bool MeshWithNormals::intersectsAABB(const tg::aabb3 &aabb) const {
    auto dimensions = Util::IntRange(0, 3);
    for (auto f : mesh.faces()) {
        for (auto i : dimensions) {
            auto hsp = normals[f];
            auto cos = hsp.normal[i];
            if (cos == 0) {continue;}
            // all 4 AABB vertices having the min value in dimension i
            for (auto p : {aabb.min, tg::pos3(
                i == 1 ? aabb.max.x : aabb.min.x,
                i == 2 ? aabb.max.y : aabb.min.y,
                i == 0 ? aabb.max.z : aabb.min.z
            ), tg::pos3(
                i == 2 ? aabb.max.x : aabb.min.x,
                i == 0 ? aabb.max.y : aabb.min.y,
                i == 1 ? aabb.max.z : aabb.min.z
            ), tg::pos3(
                i != 0 ? aabb.max.x : aabb.min.x,
                i != 1 ? aabb.max.y : aabb.min.y,
                i != 2 ? aabb.max.z : aabb.min.z
            )}) {
                auto param = (hsp.dis - tg::dot(p, hsp.normal)) / cos;
                if (param >= 0 && param <= (aabb.max[i] - aabb.min[i])) {
                    return true;
                }
            }
        }
    }
    for (auto e : mesh.edges()) {
        if (tg::intersects(
            tg::segment3(position[e.vertexA()], position[e.vertexB()]), aabb)
        ) {return true;}
    }
    return false;
}

static std::optional<float> intersectionParam(const tg::halfspace3 &hsp, const tg::segment3 &seg) {
    auto vec = seg.pos1 - seg.pos0;
    auto div = tg::dot(hsp.normal, vec);
    if (div == 0) {return std::nullopt;}  // segment parallel to face
    auto dist = hsp.dis - tg::dot(hsp.normal, seg.pos0);
    return dist / div;
}

bool MeshWithNormals::faceIntersectsSegment(pm::face_handle face, const tg::segment3 &seg, float epsilon) const {
    const auto &hsp = normals[face];
    auto param = intersectionParam(normals[face], seg);
    if (!param || *param < -epsilon || *param > 1.f + epsilon) {
        return false;
    }
    auto p = seg[*param];
    TG_ASSERT(std::abs(tg::dot(p, hsp.normal) - hsp.dis) < 0.001);
    for (auto h : face.halfedges()) {
        auto from = position[h.vertex_from()], to = position[h.vertex_to()];
        if (tg::dot(p - from, tg::cross(hsp.normal, to - from)) < -epsilon) {
            return false;
        }
    }
    return true;
}

bool CollisionMesh::collides(CollisionQuery &query) const {
    const auto checkAABB = [&query] (const tg::aabb3 &a, int level) {
        query.nAABBChecks += 1;
        return query.intersectsAABB(a);
    };
    bool foundCollision = false;
    this->edgeTree.visit(checkAABB, [&] (const tg::segment3 &seg) {
        for (auto f : query.mesh.faces()) {
            query.nFaceChecks += 1;
            if (query.faceIntersectsSegment(f, seg)) {
                foundCollision = true;
                return false;
            }
        }
        return true;
    });
    if (foundCollision) {return true;}
    this->faceTree.visit(checkAABB, [&] (const IndexedFace &face) {
        auto f = mesh.handle_of(face.idx);
        for (auto e : query.mesh.edges()) {
            query.nFaceChecks += 1;
            if (faceIntersectsSegment(f, tg::segment3(
                query.position[e.vertexA()],
                query.position[e.vertexB()]
            ))) {
                foundCollision = true;
                return false;
            }
        }
        return true;
    });
    return foundCollision;
}

Collider::Collider(ECS::ECS &ecs, tg::vec3 height, float radius) : mECS{ecs}, mHeight{height}, mRadius{radius} {
    for (auto i : Util::IntRange(0, 8)) {query.mesh.vertices().add();}
    static constexpr std::initializer_list<std::array<int, 4>> arr {
        {0, 2, 3, 1},  // front
        {4, 5, 7, 6},  // back
        {2, 0, 4, 6},  // left
        {3, 7, 5, 1},  // right
        {4, 0, 1, 5},  // bottom
        {6, 7, 3, 2},  // top
    };
    for (auto &a : arr) {
        auto v = [&] (int i) {
            return query.mesh.handle_of(pm::vertex_index(a[i]));
        };
        query.mesh.faces().add(v(0), v(1), v(2), v(3));
    }
}

bool Collider::segmentObstructed(const tg::segment3 &seg) {
    query.nObstructionTests += 1;
    auto segVec = seg.pos1 - seg.pos0;
    auto rightVec = tg::cross(segVec, mHeight);
    auto lensq = tg::length_sqr(rightVec);
    if (!(lensq > 0)) {
        // FIXME: do something reasonable in this case
        return false;
    }
    auto right = tg::dir3(rightVec / std::sqrt(lensq));
    auto fwd = tg::normalize(tg::cross(mHeight, right));
    auto up = tg::normalize(tg::cross(right, segVec));
    auto height = tg::dot(mHeight, up), fwdDist = tg::dot(segVec, fwd);
    auto fwdVal = tg::dot(seg.pos0, fwd), rightVal = tg::dot(seg.pos0, right);
    auto upVal = tg::dot(seg.pos0, up);
    std::array<tg::halfspace3, 6> planes {
        tg::halfspace3(fwd, fwdVal + fwdDist),
        tg::halfspace3(-fwd, -fwdVal + mRadius),
        tg::halfspace3(-right, -rightVal + mRadius),
        tg::halfspace3(right, rightVal + mRadius),
        tg::halfspace3(-up, -upVal),
        tg::halfspace3(up, upVal + height + mRadius)
    };
    std::array<tg::pos3, 8> pos;
    for (size_t i : Util::IntRange(size_t(0), pos.size())) {
        auto v = query.mesh.handle_of(pm::vertex_index(i));
        auto iter = v.faces().begin();
        auto p0 = tg::plane_of(planes[(*iter).idx.value]); ++iter;
        auto p1 = tg::plane_of(planes[(*iter).idx.value]); ++iter;
        auto p2 = tg::plane_of(planes[(*iter).idx.value]);
        auto ints = tg::intersection(p0, p1, p2);
        TG_ASSERT(ints.has_value());
        pos[i] = *ints;
    }
    for (auto &pair : mObjects) {
        auto [rigid, mesh] = pair;
        auto mat = tg::mat4x3(~rigid);
        query.seg.pos0 = tg::pos3(mat * tg::vec4(seg.pos0, 1));
        query.seg.pos1 = tg::pos3(mat * tg::vec4(seg.pos1, 1));
        for (size_t i : Util::IntRange(size_t(0), pos.size())) {
            query.position[pm::vertex_index(i)] = tg::pos3(mat * tg::vec4(pos[i], 1.f));
        }
        for (size_t i : Util::IntRange(size_t(0), planes.size())) {
            auto handle = query.mesh.handle_of(pm::face_index(i));
            auto dir = tg::dir3(mat * tg::vec4(planes[i].normal, 0.f));
            query.normals[pm::face_index(i)] = tg::halfspace3(
                dir, tg::dot(dir, query.position[handle.any_vertex()])
            );
        }
        if (mesh.collides(query)) {
            query.rejected.push_back(seg);
            return true;
        }
    }
    return false;
}
