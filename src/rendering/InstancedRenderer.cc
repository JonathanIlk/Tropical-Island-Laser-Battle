// SPDX-License-Identifier: MIT
#include "InstancedRenderer.hh"
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/objects/VertexArray.hh>

#include <Mesh3D.hh>
#include <ECS/Misc.hh>
#include <rendering/MainRenderPass.hh>

InstancedRenderer::InstanceData::InstanceData(const ECS::Rigid &rigid, uint32_t id) : pickID{id} {
    auto mat = rigid.transform_mat();
    model_col0 = mat[0];
    model_col1 = mat[1];
    model_col2 = mat[2];
    model_col3 = mat[3];
}

std::vector<glow::ArrayBufferAttribute> InstancedRenderer::InstanceData::attributes() {
    return {
        {&InstanceData::model_col0, "aModel_col0"},
        {&InstanceData::model_col1, "aModel_col1"},
        {&InstanceData::model_col2, "aModel_col2"},
        {&InstanceData::model_col3, "aModel_col3"},
        {&InstanceData::pickID, "aPickID"},
    };
}

void InstancedRenderer::render(MainRenderPass& pass) {
    for (auto && tup : shaderInfos) {
        auto &[shader, shaderInfo] = tup;
        shader->setUniformBuffer("uLighting", pass.lightingUniforms);
        auto usedShader = shader->use();
        pass.applyCommons(usedShader);
        pass.applyTime(usedShader);
        pass.applyWind(usedShader);
        usedShader["uTexAlbedo"] = shaderInfo.albedoTex;

        for(auto &vaoInfo : shaderInfo.vaoInfos) {
            auto &wind = vaoInfo->windSettings;
            usedShader["uObjectHeight"] = wind.objectHeight;
            usedShader["uWindAffectFrom"] = wind.startFromHeight;
            vaoInfo->vao->bind().draw();
        }
    }
}
InstancedRenderer::VaoInfo& InstancedRenderer::loadVaoForRendering(const glow::SharedProgram& shader, const glow::SharedTexture2D& albedoTex, const std::string& filePath)
{
    Mesh3D mesh;
    mesh.loadFromFile(filePath, true,false);
    glow::SharedVertexArray vao = mesh.createVertexArray();
    auto fluffTransformations = glow::ArrayBuffer::create(InstanceData::attributes());
    fluffTransformations->setDivisor(1);
    vao->bind().attach(fluffTransformations);

    auto meshWindSettings = Wind::PerMeshSettings();
    meshWindSettings.objectHeight = mesh.maxExtents.y;

    auto vaoInfo = std::make_unique<VaoInfo>();
    vaoInfo->vao = vao;
    vaoInfo->instancedDataBuffer = fluffTransformations;
    vaoInfo->windSettings = meshWindSettings;

    auto &shaderInfo = shaderInfos[shader];
    shaderInfo.albedoTex = albedoTex;
    return *shaderInfo.vaoInfos.emplace_back(std::move(vaoInfo));
}

void InstancedRenderer::updateBuffers() {
    for (auto && tup : shaderInfos)
    {
        auto& [shader, shaderInfo] = tup;
        for (auto& vaoInfo : shaderInfo.vaoInfos)
        {
            auto &instanceData = vaoInfo->instanceData;
            vaoInfo->instancedDataBuffer->bind().setData(instanceData);
            instanceData.clear();
        }
    }
}
