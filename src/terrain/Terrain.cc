// SPDX-License-Identifier: MIT
#include "Terrain.hh"
#include <cinttypes>

#include <polymesh/algorithms/delaunay.hh>
#include <polymesh/algorithms/triangulate.hh>
#include <glow/glow.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/objects/VertexArray.hh>
#include <imgui/imgui.h>

#include <external/SimplexNoise.h>

#include <MathUtil.hh>
#include <ECS/Join.hh>
#include <ECS/Misc.hh>
#include <navmesh/NavMesh.hh>
#include <rendering/MainRenderPass.hh>
#include <rendering/MeshViz.hh>
#include "TerrainMaterial.hh"
#include "Water.hh"
#include <startsequence/StartSequence.hh>

using namespace Terrain;

Instance::Instance(std::mt19937 &rng) {
    this->noiseOffset = std::uniform_real_distribution<float> {0.f, 10000.f}(rng);

    this->noiseScale = 120;
    this->noiseOctaves = 8;
    this->mountainHeight = 10;
    // General noise for creating valleys and hills
    this->landscapeNoise = SimplexNoise(1/ noiseScale, 0.5f, 1.99f, 0.5f);
    // smaller noise four roughing up terrain
    this->roughnessNoise = SimplexNoise(10/ noiseScale, 0.5f, 1.99f, 0.5f);

    this->segmentsAmount = 200;
    this->segmentSize = 4;
    this->center = tg::pos2((float)segmentsAmount*segmentSize / 2, (float)segmentsAmount*segmentSize / 2);

    auto mesh = std::make_unique<pm::Mesh>();
    this->posAttr = mesh->vertices().make_attribute<tg::pos3>();

    for (uint32_t x = 0; x < segmentsAmount; x++) {
        for (uint32_t z = 0; z < segmentsAmount; z++) {
            auto v = mesh->vertices().add();
            this->posAttr[v] = getVertexPositionForSegment(x, z);
        }
    }

    auto vertex_index_at = [&](uint32_t ix, uint32_t iz) {
        // we know in which order we created vertices
        // thus we can compute index directly
        return mesh->vertices()[ix * segmentsAmount + iz];
    };

    // Connect the created vertices to make faces.
    for (uint32_t x = 0; x < segmentsAmount-1; x++) {
        for (uint32_t z = 0; z < segmentsAmount-1; z++) {
            mesh->faces().add(
                vertex_index_at(x + 0, z + 0),
                vertex_index_at(x + 0, z + 1),
                vertex_index_at(x + 1, z + 1),
                vertex_index_at(x + 1, z + 0)
            );
        }
    }

    pm::triangulate_naive(*mesh);
    pm::make_delaunay(*mesh, posAttr);

    this->mesh = std::move(mesh);
}

Rendering::Rendering(const Instance &terr) {
    std::vector<float> heights;
    heights.reserve(terr.segmentsAmount * terr.segmentsAmount);
    for (auto v : terr.mesh->all_vertices()) {
        heights.push_back(terr.posAttr[v].y);
    }

    std::vector<tg::color3> colors;
    colors.reserve(terr.segmentsAmount * terr.segmentsAmount);
    for (auto v : terr.mesh->all_vertices()) {
        auto material = terr.getMaterialForPosition(terr.posAttr[v]);
        colors.push_back(TerrainMaterial::table[material].randomTint(v.idx.value));
    }

    std::vector<uint32_t> indices;
    indices.reserve(terr.mesh->faces().size());
    for (auto f : terr.mesh->faces()) {
        for (auto h : f.halfedges()) {indices.push_back(h.vertex_to().idx.value);}
    }
    auto buf = glow::ArrayBuffer::create("aHeight", heights);
    auto idxbuf = glow::ElementArrayBuffer::create(indices);
    auto colorbuf = glow::ArrayBuffer::create("color", colors);
    this->vao = glow::VertexArray::create({buf, colorbuf}, idxbuf);
}

TerrainMaterial::ID Instance::getMaterialForPosition(tg::pos3 pos) const {
    if(pos.y > mountainHeight * 0.5f) {
        return TerrainMaterial::Rock;
    } else if(pos.y > waterLevel + 3.) {
        return TerrainMaterial::Grass;
    } else {
        return TerrainMaterial::Sand;
    }
}

tg::pos3 Instance::getVertexPositionForSegment(int x, int z) const
{
    float xPos = (float)x * segmentSize;
    float zPos = (float)z * segmentSize;
    float pointElevation = getElevationAtPos(xPos, zPos);

    return tg::pos3((float)xPos, pointElevation, (float)zPos);
}
float Instance::getElevationAtPos(float xPos, float zPos) const
{
    float landscapeElevation = landscapeNoise.fractal(noiseOctaves, (float)xPos + noiseOffset, (float)zPos + noiseOffset) * mountainHeight;
    float roughnessElevation = roughnessNoise.fractal(1, (float)xPos + noiseOffset, (float)zPos + noiseOffset) * 0;
    float noiseResult = landscapeElevation + roughnessElevation;

    float islandFallOff = getIslandFalloff(xPos, zPos);

    float elevation = noiseResult - islandFallOff;

    float borderElevation = -6.0f;

    float terrainRadius = segmentSize * segmentsAmount / 2;
    float minDistanceFromBorder = tg::min(terrainRadius - std::abs(xPos - terrainRadius), terrainRadius - std::abs(zPos - terrainRadius));

    float flatten = 1- tg::smoothstep(0.0f, 50.0f, minDistanceFromBorder);
    elevation = tg::lerp(elevation, -waterdepth, flatten);

    return elevation;
}
float Instance::getIslandFalloff(float xPos, float zPos) const {
    // Radius is half the size so every water has land below it.
    float islandRadius = (((float)segmentsAmount * segmentSize) / 2) * 0.5f;
    float falloff = (-1 * tg::clamp(islandRadius - tg::distance(tg::pos2(xPos, zPos), center), -waterdepth, 0) * beachSteepness);
    return falloff;
}

System::System(ECS::ECS &ecs) : mECS{ecs} {
    mShader = glow::Program::createFromFiles({"../data/shaders/terrain.vsh", "../data/shaders/terrain.gsh", "../data/shaders/terrain.fsh"});
}

void System::renderMain(MainRenderPass &pass, const float minAlpha) {
    ECS::entity ent = 0;
    mShader->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto shader = mShader->use();
    pass.applyCommons(shader);
    shader["uMinAlpha"] = minAlpha;

    auto range = ECS::Join(pass.snap->rigids, mECS.terrains, mECS.terrainRenderings);
    for (auto &&tup : range) {
        auto &[wo, terr, rend, id] = tup;
        shader["uModel"] = wo.transform_mat();
        shader["uPickID"] = id;
        shader["uRows"] = terr.segmentsAmount;
        shader["uSegmentSize"] = terr.segmentSize;
        shader["uTerrainRadius"] = (terr.segmentSize * terr.segmentsAmount) / 2;
        auto va = rend.vao->bind();
        va.draw();
    }
}

void System::renderingEditorUI(ECS::entity ent, Instance &terr) {
    auto rend_iter = mECS.terrainRenderings.find(ent);
    if (rend_iter != mECS.terrainRenderings.end()) {
        bool want_rendering = true;
        if (ImGui::Checkbox("Terrain Rendering", &want_rendering) && !want_rendering) {
            mECS.terrainRenderings.erase(rend_iter);
        } else {
            ImGui::TextUnformatted("Placeholder for pure rendering options");
        }
    } else {
        bool want_rendering = false;
        if (ImGui::Checkbox("Rendering", &want_rendering) && want_rendering) {
            mECS.terrainRenderings.emplace(ent, Rendering(terr));
        }
    }
}

void System::editorUI(ECS::entity ent) {
    auto terr_iter = mECS.terrains.find(ent);
    if (terr_iter == mECS.terrains.end()) {
        ImGui::Text("Entity %" PRIu32 " is associated with the Terrain editor, but is not a terrain object", ent);
        return;
    }
    auto &terr = terr_iter->second;

    ImGui::TextUnformatted("Placeholder for Terrain options");

    auto terr_wo_iter = mECS.staticRigids.find(ent);
    if (terr_wo_iter != mECS.staticRigids.end())
    {
        if (ImGui::Button("Startsequence"))
        {
            mECS.startSequenceSys->startSequence(terr, terr_wo_iter->second);
        }
    }


    renderingEditorUI(ent, terr);
    mECS.navMeshSys->editorUI(ent);
}
