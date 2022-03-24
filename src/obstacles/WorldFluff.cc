// SPDX-License-Identifier: MIT
#include "WorldFluff.hh"
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <polymesh/Mesh.hh>

#include <Game.hh>
#include <MathUtil.hh>
#include <Util.hh>
#include <ECS/Join.hh>
#include <terrain/Terrain.hh>
#include <terrain/TerrainMaterial.hh>

using namespace WorldFluff;

System::System(Game& game) : mECS{game.mECS} {
    auto &tex = game.mSharedResources.colorPaletteTex;
    auto &shaderFlat = game.mSharedResources.flatInstanced;
    auto &shaderWind = game.mSharedResources.flatWindy;

    struct Template {
        const char *meshPath;
        const glow::SharedProgram &shader;
        std::array<float, TerrainMaterial::NumMaterials> weight;
    };
    for (auto &templ : std::initializer_list<Template> {
        {"../data/meshes/grass1.obj", shaderWind, {0.f, 1.f, 1.f}},
        {"../data/meshes/grass2.obj", shaderWind, {.0f, 1.f, 1.f}},
        {"../data/meshes/smallrocks1.obj", shaderFlat, {1.f, 1.f, 1.f}}
    }) {
        auto &vaoInfo = mInstancedRenderer.loadVaoForRendering(
            templ.shader, tex, templ.meshPath
        );
        auto idx = mTypes.size();
        mTypes.emplace_back(vaoInfo);
        for (auto t : Util::IntRange(TerrainMaterial::NumMaterials)) {
            auto w = templ.weight[t];
            if (w > 0)  {mTypesForTerrain[t].values.emplace_back(w, idx);}
        }
    }
    for (auto &tt : mTypesForTerrain) {tt.update();}
}

void System::spawnFluff(ECS::Rigid &wo, Terrain::Instance &terr, std::mt19937& rng)
{
    auto xform = wo.transform_mat();

    for(auto & fluffFace : randomlySelectFluffPositions(terr, rng)) {
        auto vertexIter = fluffFace.vertices().begin();
        tg::pos3& p1 = terr.posAttr[*vertexIter++];
        tg::pos3& p2 = terr.posAttr[*vertexIter++];
        tg::pos3& p3 = terr.posAttr[*vertexIter];
        auto fluffPosition = Util::randomPositionOnTriangle(rng, p1, p2, p3);
        auto normal = Util::triangleNormal(p1, p2, p3);
        if(fluffPosition.y < terr.waterLevel) {continue;}

        auto material = terr.getMaterialForPosition(fluffPosition);
        auto &tt = mTypesForTerrain.at(material);
        auto &type = mTypes.at(tt(rng));
        tg::quat alignToNormal = Util::fromToRotation(tg::vec3::unit_y, normal);
        tg::quat randomRotation = tg::quat::from_axis_angle(tg::dir3(tg::vec3::unit_y), tg::angle::from_degree(std::uniform_int_distribution<int>(0, 360)(rng)));
        auto worldPos = tg::pos3(xform * tg::vec4(fluffPosition.x, fluffPosition.y, fluffPosition.z, 1));

        ECS::entity ent = mECS.newEntity();
        mECS.instancedRigids.emplace(ent, ECS::Rigid(
            worldPos, wo.rotation * alignToNormal * randomRotation
        ));
        mECS.worldFluffs.emplace(ent, type);
    }

    for (auto &&tup : ECS::Join(mECS.instancedRigids, mECS.worldFluffs)) {
        auto &[rig, type, id] = tup;
        type.vaoInfo.instanceData.emplace_back(rig, id);
    }
    mInstancedRenderer.updateBuffers();
}

std::vector<polymesh::face_handle> System::randomlySelectFluffPositions(Terrain::Instance& terr, std::mt19937& rng) const
{
    auto allFaces = terr.mesh->faces().to_vector();
    std::vector<polymesh::face_handle> chosen_positions;

    size_t fluffAmount = allFaces.size() * fluffDensity;

    std::sample(
        allFaces.begin(),
        allFaces.end(),
        std::back_inserter(chosen_positions),
        fluffAmount,
        rng
        );
    return chosen_positions;
}

void System::renderMain(MainRenderPass& pass) {
    mInstancedRenderer.render(pass);
}
