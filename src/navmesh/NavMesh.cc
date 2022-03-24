// SPDX-License-Identifier: MIT
#include "NavMesh.hh"
#include <cinttypes>
#include <vector>
#include <queue>

#include <typed-geometry/tg-std.hh>
#include <glow/common/log.hh>
#include <glow/objects.hh>
#include <imgui/imgui.h>

#include <external/lowbias32.hh>

#include <ECS/Join.hh>
#include <rendering/MeshViz.hh>
#include <rtree/RStar.hh>
#include <terrain/Terrain.hh>

using namespace NavMesh;

namespace {
struct Crossing {
    pm::edge_index edge;
    uint32_t pos;

    bool operator==(const Crossing &other) const {
        return edge == other.edge && pos == other.pos;
    }
};
}

template<>
class std::hash<Crossing> {
public:
    size_t operator() (const Crossing &c) const {
        return lowbias32(c.pos ^ lowbias32(c.edge.value));
    }
};

static tg::aabb3 faceAABB(pm::face_handle f, pm::vertex_attribute<tg::pos3> &position) {
    auto min = position[f.any_vertex()], max = min;
    for (auto v : f.vertices()) {
        auto pos = position[v];
        min = tg::min(pos, min), max = tg::max(pos, max);
    }
    return {min, max};
}

tg::pos3 NavMesh::Instance::edgeLerp(const pm::edge_handle &edge, float param) const {
    return tg::lerp(
        this->worldPos[edge.vertexA()],
        this->worldPos[edge.vertexB()],
        param
    );
}

std::vector<std::pair<pm::halfedge_index, float>> Instance::navigate(const RouteRequest &req, uint32_t nsteps, Obstacle::Collider &collider) {
    assert(req.start_face != req.end_face);  // empty vector is the error result
    auto end_face = this->mesh->handle_of(req.end_face);
    auto start_face = this->mesh->handle_of(req.start_face);
    struct CrossInfo {
        float distance;  // geometric distance from this edge to the target
        float lowerBound;  // lower bound for the path length up to here
        pm::halfedge_index predecessor;
        uint32_t predPos;
    };
    std::unordered_map<Crossing, CrossInfo> crossInfo;
    struct WorkListEntry {
        float lowerBound;  // lower bound for the entire path, not just the given edge
        pm::halfedge_index halfedge;
        uint32_t crossPos;
        bool operator<(const WorkListEntry &other) const {
            return lowerBound > other.lowerBound;
        }
    };
    float stepSize = 1.f / nsteps, posBase = stepSize / 2;
    std::priority_queue<WorkListEntry> workList;
    auto aabb = faceAABB(end_face, this->worldPos);
    collider.collectObjects(aabb);
    for (auto h : end_face.halfedges()) {
        if (h.opposite().is_boundary()) {continue;}
        auto a = this->worldPos[h.edge().vertexA()];
        auto b = this->worldPos[h.edge().vertexB()];
        for (uint32_t i = 0; i < nsteps; ++i) {
            auto p = tg::lerp(a, b, i * stepSize + posBase);
            auto lowerBound = tg::distance(req.end, p);
            auto dist = tg::distance(p, req.start);
            if (collider.segmentObstructed(tg::segment3(p, req.end))) {continue;}
            crossInfo.emplace(Crossing({h.edge(), i}), CrossInfo({dist, lowerBound, {}}));
            workList.push({lowerBound + dist, h, i});
        }
    }
    pm::halfedge_handle best_he;
    uint32_t bestPos = 0;
    size_t nconnections = 0;
    while (!workList.empty()) {
        auto top = workList.top();
        workList.pop();
        auto he = this->mesh->halfedges()[top.halfedge];
        auto edge = he.edge();
        Crossing crossing = {he.edge(), top.crossPos};
        TG_ASSERT(!he.is_boundary());
        auto &info = crossInfo[crossing];
        // discard stale entries
        if (top.lowerBound > info.lowerBound + info.distance) {continue;}

        auto p = this->edgeLerp(edge, stepSize * top.crossPos + posBase);
        aabb = faceAABB(he.opposite().face(), this->worldPos);
        collider.collectObjects(aabb);
        if (he.opposite_face() == start_face) {
            if (!collider.segmentObstructed(tg::segment3(req.start, p))) {
                best_he = he;
                bestPos = top.crossPos;
                break;
            }
        }
        for (auto h = he.opposite().next(); h != he.opposite(); h = h.next()) {
            if (h.opposite().is_boundary()) {continue;}
            auto edge_ = h.edge();
            for (uint32_t i = 0; i < nsteps; ++i) {
                Crossing crossing_ = {edge_, i};
                auto p_ = this->edgeLerp(edge_, i * stepSize + posBase);
                if (collider.segmentObstructed(tg::segment3(p_, p))) {continue;}
                auto lowerBound = info.lowerBound + tg::distance(p, p_);
                auto iter = crossInfo.find(crossing_);
                if (iter == crossInfo.end()) {  // open the crossing
                    auto dist = tg::distance(p_, req.start);
                    crossInfo.emplace(crossing_, CrossInfo({dist, lowerBound, he, top.crossPos}));
                    workList.push({lowerBound + dist, h, i});
                } else {
                    auto &info_ = iter->second;
                    if (info_.lowerBound > lowerBound) {  // lower bound improved
                        info_.lowerBound = lowerBound;
                        info_.predecessor = he;
                        info_.predPos = top.crossPos;
                        workList.push({lowerBound + iter->second.distance, h, i});
                    }
                }
                nconnections += 1;
            }
        }
    }
    glow::info() << crossInfo.size() << " crossings opened";
    glow::info() << nconnections << " connections tested";
    std::vector<std::pair<pm::halfedge_index, float>> res;
    while (best_he.is_valid()) {
        auto pos = bestPos;
        if (best_he.edge().vertexA() != best_he.vertex_to()) {
            pos = nsteps - 1 - pos;
        }
        res.push_back({best_he.opposite().idx, pos * stepSize + posBase});
        Crossing crossing = {best_he.edge(), bestPos};
        auto &info = crossInfo[crossing];
        best_he = this->mesh->handle_of(info.predecessor);
        bestPos = info.predPos;
    }
    return res;
}

Instance::Instance(const ECS::Rigid &wo, const Terrain::Instance &terrain) {
    this->mesh->copy_from(*terrain.mesh);
    this->localPos.copy_from(terrain.posAttr);
    auto xform = wo.transform_mat();
    for (auto v : this->mesh->all_vertices()) {
        this->worldPos[v] = tg::pos3(xform * tg::vec4(this->localPos[v.idx], 1));
    }
    auto test = [&] (pm::vertex_handle v) {
        return this->worldPos[v].y >= terrain.waterLevel;
    };
    for (auto f : this->mesh->faces()) {
        bool keep = false;
        for (auto v : f.vertices()) {
            if (test(v)) {keep = true;}
        }
        if (!keep) {this->mesh->faces().remove(f);}
    }

    // clean up edges and vertices that are only used by removed faces
    for (auto e : this->mesh->edges()) {
        if (e.halfedgeA().is_boundary() && e.halfedgeB().is_boundary()) {
            this->mesh->edges().remove(e);
        }
    }
    for (auto v : this->mesh->vertices()) {
        if (v.is_isolated()) {this->mesh->vertices().remove(v);}
    }
    this->mesh->compactify();
    for (auto f : this->mesh->faces()) {
        auto aabb = faceAABB(f, this->worldPos);
        decltype(faceTree)::RStarInserter::insert(this->faceTree, {aabb, f.idx});
    }
}

void System::editorUI(ECS::entity ent) {
    auto terr_iter = mECS.terrains.find(ent);
    if (terr_iter == mECS.terrains.end()) {
        ImGui::Text("Entity %" PRIu32 " is associated with the NavMesh editor, but is not a terrain object", ent);
        return;
    }
    auto &terr = terr_iter->second;
    auto rig_iter = mECS.simSnap->rigids.find(ent);
    if (rig_iter == mECS.simSnap->rigids.end()) {
        ImGui::Text("Entity %" PRIu32 " is associated with the NavMesh editor, but is not a rigid body", ent);
        return;
    }
    auto &wo = rig_iter->second;

    auto nav_iter = mECS.navMeshes.find(ent);
    if (nav_iter != mECS.navMeshes.end()) {
        bool want_rendering = true;
        if (ImGui::Checkbox("NavMesh", &want_rendering) && !want_rendering) {
            mECS.navMeshes.erase(nav_iter);
        } else {
            auto &nav = nav_iter->second;
            ImGui::TextUnformatted("Placeholder NavMesh options");
            auto viz_iter = mECS.vizMeshes.find(ent);
            if (viz_iter != mECS.vizMeshes.end()) {
                bool want_rendering = true;
                if (ImGui::Checkbox("Mesh visualization", &want_rendering) && !want_rendering) {
                    mECS.vizMeshes.erase(viz_iter);
                } else {
                    ImGui::TextUnformatted("Placeholder MeshViz options");
                }
            } else {
                bool want_rendering = false;
                if (ImGui::Checkbox("Mesh Visualization", &want_rendering) && want_rendering) {
                    mECS.vizMeshes.emplace(ent, MeshViz::Instance(*nav.mesh, nav.localPos, .2f));
                }
            }
        }
    } else {
        bool want_rendering = false;
        if (ImGui::Checkbox("NavMesh", &want_rendering) && want_rendering) {
            mECS.navMeshes.emplace(ent, Instance(wo, terr));
        }
    }
}

std::optional<float> Instance::intersectionTest(pm::face_handle f, const tg::ray3 &ray) const {
    std::optional<float> leastDepth;
    auto iter1 = f.vertices().begin();
    while (iter1 != f.vertices().end()) {
        auto a = worldPos[*iter1];
        auto iter2 = iter1;
        while (true) {
            ++iter2;
            if (iter2 == f.vertices().end()) {break;}
            auto b = worldPos[*iter2];
            auto iter3 = iter2;
            while (true) {
                ++iter3;
                if (iter3 == f.vertices().end()) {break;}
                auto c = worldPos[*iter3];
                auto hits = tg::intersection_parameter(ray, tg::triangle(a, b, c));
                if (!hits.any()) {continue;}
                auto depth = hits.first();

                if (!leastDepth.has_value() || depth < leastDepth) {
                    leastDepth = depth;
                }
            }
        }
        ++iter1;
    }
    return leastDepth;
}

std::optional<std::pair<pm::face_index, float>> Instance::intersect(const tg::ray3 &ray) const {
    std::optional<std::pair<pm::face_index, float>> res;
    faceTree.visit([&] (const tg::aabb3 &a, decltype(faceTree)::level_t) {
        return tg::intersects(a, ray);
    }, [&] (const NavMesh::FaceInfo &a) {
        if (!tg::intersects(a.aabb, ray)) {return true;}
        auto f = mesh->handle_of(a.idx);
        auto depth = intersectionTest(f, ray);
        if (depth && (!res || res->second > *depth)) {
            res = {a.idx, *depth};
        }
        return true;
    });
    return res;
}

std::optional<std::tuple<ECS::entity, pm::face_index, float>> System::intersect(const tg::ray3 &ray) {
    std::optional<std::tuple<ECS::entity, pm::face_index, float>> res;
    for (auto &&nav_ : mECS.navMeshes) {
        auto hit = nav_.second.intersect(ray);
        if (hit && (!res || hit->second < std::get<2>(*res))) {
            res = {{nav_.first, hit->first, hit->second}};
        }
    }
    return res;
}

std::optional<std::pair<pm::face_index, float>> Instance::closestPoint(tg::pos3 pos) const {
    auto maxDist = 1.f;
    std::optional<std::pair<pm::face_index, float>> res;
    this->faceTree.visit([&, pos] (const tg::aabb3 &a, decltype(this->faceTree)::level_t) {
        if (!res) {return true;}
        auto dist = tg::distance(a, pos);
        if (dist > maxDist) {return false;}
        return dist < res->second;
    }, [&, pos] (const NavMesh::FaceInfo &a) {
        if (res && tg::distance(a.aabb, pos) > res->second) {return true;}
        auto f = this->mesh->handle_of(a.idx);
        auto iter1 = f.vertices().begin();
        while (iter1 != f.vertices().end()) {
            auto a = this->worldPos[*iter1];
            auto iter2 = iter1;
            while (true) {
                ++iter2;
                if (iter2 == f.vertices().end()) {break;}
                auto b = this->worldPos[*iter2];
                auto iter3 = iter2;
                while (true) {
                    ++iter3;
                    if (iter3 == f.vertices().end()) {break;}
                    auto c = this->worldPos[*iter3];
                    auto dist = tg::distance(tg::triangle(a, b, c), pos);

                    if (!res || dist < res->second) {
                        res = {f.idx, dist};
                        glow::info() << "replacing incumbent nearest";
                    }
                }
            }
            ++iter1;
        }
        return true;
    });
    return res;
}

tg::vec3 Instance::faceNormal(pm::face_index f) const {
    auto he = mesh->handle_of(f).any_halfedge();
    auto a = worldPos[he.prev().vertex_from()];
    auto b = worldPos[he.vertex_from()];
    auto c = worldPos[he.vertex_to()];
    return tg::cross(a - b, a - c);
}
