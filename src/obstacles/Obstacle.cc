// SPDX-License-Identifier: MIT
#include "Obstacle.hh"
#include <cinttypes>
#include <random>
#include "../../extern/typed-geometry/src/typed-geometry/feature/quat.hh"

#include <polymesh/formats/obj.hh>
#include <imgui/imgui.h>

#include <ECS/Join.hh>
#include <Game.hh>
#include <Util.hh>
#include <animation/AnimatorManager.hh>
#include <rendering/MeshViz.hh>
#include <rtree/RStar.hh>
#include <terrain/Terrain.hh>
#include <terrain/TerrainMaterial.hh>
#include <environment/Parrot.hh>

using namespace Obstacle;

System::System(Game& game) : mECS{game.mECS}, mSharedResources(game.mSharedResources) {
    auto &tex = game.mSharedResources.colorPaletteTex;
    auto &shaderFlat = game.mSharedResources.flatInstanced;
    auto &shaderWind = game.mSharedResources.flatWindy;

    struct TypeTemplate {
        const int id;
        const char *meshPath, *colliderPath;
        const glow::SharedProgram &shader;
        float windHeightFraction;
        bool highCover;
        std::array<float, TerrainMaterial::NumMaterials> weights;
    };
    for (const auto &templ : std::initializer_list<TypeTemplate> {{
        0, "../data/meshes/palm1.obj", "../data/meshes/palm1_collider.obj",
        shaderWind, .1f, true, {0.f, 1.f, 1.f}
    }, {
        1, "../data/meshes/rock1.obj", "../data/meshes/rock1_collider.obj",
        shaderFlat, 0.f, true, {1.f, .6f, .6f}
    }, {
        2, "../data/meshes/brokenwall1.obj", "../data/meshes/brokenwall1_collider.obj",
        shaderFlat, 0.f, false, {.3f, .2f, .2f}
    }}) {
        auto &vaoInfo = mInstancedRenderer.loadVaoForRendering(
            templ.shader, tex, templ.meshPath
        );
        if (templ.windHeightFraction > 0) {
            auto &settings = vaoInfo.windSettings;
            settings.startFromHeight = settings.objectHeight * templ.windHeightFraction;
        }
        auto idx = mTypes.size();
        auto &type = mTypes.emplace_back(vaoInfo);
        type.id = templ.id;
        type.highCover = templ.highCover;
        type.collisionMesh = std::make_unique<CollisionMesh>();
        initObstacleCollider(*type.collisionMesh, templ.colliderPath);
        for (auto t : Util::IntRange(TerrainMaterial::NumMaterials)) {
            auto w = templ.weights[t];
            if (w > 0)  {mTypesForTerrain[t].values.emplace_back(w, idx);}
        }
    }
    for (auto &tt : mTypesForTerrain) {tt.update();}
}

void System::initObstacleCollider(CollisionMesh &collider, const char *meshName) const {
    pm::obj_reader<float> reader(meshName, collider.mesh);
    auto &pos = collider.position;
    pos = reader.get_positions().to<tg::pos3>();
    for (auto v : collider.mesh.vertices()) {
        decltype(collider.vertexTree)::RStarInserter::insert(
            collider.vertexTree, tg::pos3(pos[v])
        );
    }
    for (auto e : collider.mesh.edges()) {
        decltype(collider.edgeTree)::RStarInserter::insert(
            collider.edgeTree, tg::segment3({pos[e.vertexA()], pos[e.vertexB()]})
        );
    }
    for (auto f : collider.mesh.faces()) {
        auto p0 = pos[f.any_vertex()];
        tg::aabb3 aabb = {p0, p0};
        auto normal = tg::vec3::zero;
        for (auto h : f.halfedges()) {
            p0 = pos[h.prev().vertex_from()];
            auto p1 = pos[h.vertex_from()], p2 = pos[h.vertex_to()];
            normal += tg::cross(p1 - p0, p2 - p1);
            aabb.min = tg::min(aabb.min, p0);
            aabb.max = tg::max(aabb.max, p0);
        }
        auto dir = tg::normalize(normal);
        collider.normals[f] = tg::halfspace3(dir, tg::dot(dir, p0));
        decltype(collider.faceTree)::RStarInserter::insert(collider.faceTree, {aabb, f.idx});
    }
}

void System::spawnObstacles(ECS::Rigid &wo, Terrain::Instance &terr, std::mt19937& rng)
{
    auto xform = wo.transform_mat();

    for(auto &obstaclePos : randomlySelectedObstaclePositions(terr, rng)) {
        if(obstaclePos.y < terr.waterLevel) {continue;}
        auto material = terr.getMaterialForPosition(obstaclePos);
        auto &type = mTypes.at(mTypesForTerrain[material](rng));

        tg::quat randomRotation = tg::quat::from_axis_angle(tg::dir3::pos_y, tg::angle::from_degree(std::uniform_int_distribution<int>(0, 360)(rng)));
        auto worldPos = tg::pos3(xform * tg::vec4(obstaclePos.x, obstaclePos.y, obstaclePos.z, 1));
        ECS::Rigid rig = {worldPos, wo.rotation * randomRotation};

        tg::aabb3 aabb = {worldPos, worldPos};
        auto mat = tg::mat4x3(rig);
        auto &collider = *type.collisionMesh;
        for (auto v : collider.mesh.all_vertices()) {
            auto p = tg::pos(mat * tg::vec4(collider.position[v], 1));
            aabb = tg::aabb3 {tg::min(aabb.min, p), tg::max(aabb.max, p)};
        }

        ECS::entity ent = mECS.newEntity();
        mECS.obstacles.emplace(ent, type);
        mECS.instancedRigids.emplace(ent, rig);
        mECS.editables.emplace(ent, this);
        decltype(mECS.obstructions)::RStarInserter::insert(mECS.obstructions, {aabb, ent});

        if (type.id == 1)
        {
            auto isParrotSpawn = std::uniform_real_distribution<>{0.0, 1.0}(rng);
            if (isParrotSpawn <= parrotDensity)
            {
                spawnParrot(wo, randomRotation, worldPos, rng);
            }
        }
    }

    for (auto &&tup : ECS::Join(mECS.instancedRigids, mECS.obstacles)) {
        auto &[rig, type, id] = tup;
        type.vaoInfo.instanceData.emplace_back(rig, id);
    }
    mInstancedRenderer.updateBuffers();
}

void System::spawnParrot(const ECS::Rigid& wo,
                         const tg::quat& randomRotation,
                         const tg::pos3& worldPos,
                         std::mt19937& rng)
{
    // Add Parrot to rocks.
    ECS::entity parrotEnt = mECS.newEntity();
    ECS::Rigid rigid = {worldPos, wo.rotation * randomRotation};
    mECS.riggedRigids.emplace(parrotEnt, rigid);
    std::string startAnim = mSharedResources.ANIM_PARROT_IDLE;
    if (std::uniform_int_distribution{1, 10}(rng) >= 9)
    {
        startAnim = mSharedResources.ANIM_PARROT_FLY;
    }
    auto parrotInstance = RiggedMesh::Instance(&mSharedResources.mParrotMesh, startAnim);
    AnimatorManager::start(parrotInstance.animator);
    parrotInstance.animator->mAnimationTime = std::uniform_real_distribution{0.0f, 3.0f}(rng);
    mECS.riggedMeshes.emplace(parrotEnt, parrotInstance);
    mECS.parrots.emplace(parrotEnt, Parrot::Instance());
}


std::vector<tg::pos3> System::randomlySelectedObstaclePositions(Terrain::Instance& terr, std::mt19937& rng) const
{
    std::vector<tg::pos3> possible_positions = terr.posAttr.to_vector();
    std::vector<tg::pos3> chosen_positions;

    size_t obstacleAmount = possible_positions.size() * obstacleDensity;

    std::sample(
        possible_positions.begin(),
        possible_positions.end(),
        std::back_inserter(chosen_positions),
        obstacleAmount,
        rng
        );
    return chosen_positions;
}

void System::renderMain(MainRenderPass& pass) {
    mInstancedRenderer.render(pass);
}

void System::editorUI(ECS::entity ent) {
    auto obstIter = mECS.obstacles.find(ent);
    if (obstIter == mECS.obstacles.end()) {
        ImGui::Text("Entity %" PRIu32 " is associated with the NavMesh editor, but is not a terrain object", ent);
        return;
    }
    auto &type = obstIter->second;

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
            auto &collider = *type.collisionMesh;
            mECS.vizMeshes.emplace(ent, MeshViz::Instance(collider.mesh, collider.position, .01f));
        }
    }
}

System::QueryResult System::rayCast(const tg::ray3 &ray) const {
    QueryResult res;
    mECS.obstructions.visit([ray] (const tg::aabb3 &aabb, int level) {
        return tg::intersects(aabb, ray);
    }, [&] (const Obstruction &obstruction) {
        if (!tg::intersects(obstruction.aabb, ray)) {return true;}
        auto join = ECS::Join(mECS.obstacles, mECS.instancedRigids);
        auto obstacleIter = join.find(obstruction.id);
        if (obstacleIter == join.end()) {return true;}
        auto [type, rigid, id] = *obstacleIter;
        auto &collider = *type.collisionMesh;
        auto mat = tg::mat4x3(~rigid);
        auto localRay = tg::ray3(
            tg::pos3(mat * tg::vec4(ray.origin, 1)),
            tg::dir3(mat * tg::vec4(ray.dir, 0))
        );
        collider.faceTree.visit([localRay] (const tg::aabb3 &aabb, int level) {
            auto res = tg::intersects(localRay, aabb);
            return res;
        }, [&] (const IndexedFace &face) {
            if (!tg::intersects(localRay, face.aabb)) {return true;}
            auto handle = collider.mesh.handle_of(face.idx);
            auto vIter = handle.vertices().begin();
            auto vEnd = handle.vertices().end();
            auto p0 = collider.position[*vIter];
            ++vIter;
            auto p1 = collider.position[*vIter];
            ++vIter;
            while (vIter != vEnd) {
                auto p2 = collider.position[*vIter];
                auto hit = tg::intersection_parameter(localRay, tg::triangle3(p0, p1, p2));
                if (hit.any() && (!res || res->second > hit.first())) {
                    res = {{obstruction.id, hit.first()}};
                }
                ++vIter;
                p1 = p2;
            }
            return true;
        });
        return true;
    });
    return res;
}

System::QueryResult System::closest(const tg::pos3 &pos) const {
    QueryResult res;
    mECS.obstructions.visit([pos, &res] (const tg::aabb3 &aabb, int level) {
        return !res || tg::distance(aabb, pos) < res->second;
    }, [&] (const Obstruction &obstruction) {
        if (res && tg::distance(obstruction.aabb, pos) >= res->second) {return true;}
        auto join = ECS::Join(mECS.obstacles, mECS.instancedRigids);
        auto obstacleIter = join.find(obstruction.id);
        if (obstacleIter == join.end()) {return true;}
        auto [type, rigid, id] = *obstacleIter;
        auto &collider = *type.collisionMesh;
        auto mat = tg::mat4x3(~rigid);
        auto localPos = tg::pos3(mat * tg::vec4(pos, 1));
        collider.faceTree.visit([localPos, &res] (const tg::aabb3 &aabb, int level) {
            return !res || tg::distance(aabb, localPos) < res->second;
        }, [&] (const IndexedFace &face) {
            if (res && tg::distance(face.aabb, localPos) >= res->second) {return true;}
            auto handle = collider.mesh.handle_of(face.idx);
            auto vIter = handle.vertices().begin();
            auto vEnd = handle.vertices().end();
            auto p0 = collider.position[*vIter];
            ++vIter;
            auto p1 = collider.position[*vIter];
            ++vIter;
            while (vIter != vEnd) {
                auto p2 = collider.position[*vIter];
                auto dist = tg::distance(tg::triangle3(p0, p1, p2), localPos);
                if (!res || res->second > dist) {
                    res = {{obstruction.id, dist}};
                }
                ++vIter;
                p1 = p2;
            }
            return true;
        });
        return true;
    });
    return res;
}
