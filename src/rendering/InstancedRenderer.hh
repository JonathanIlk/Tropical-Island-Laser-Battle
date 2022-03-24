// SPDX-License-Identifier: MIT
#pragma once
#include <map>
#include <string>
#include <vector>

#include <glow/fwd.hh>

#include <fwd.hh>
#include <environment/Wind.hh>

struct InstancedRenderer {
    struct InstanceData;
    struct VaoInfo {
        glow::SharedVertexArray vao;
        glow::SharedArrayBuffer instancedDataBuffer;
        Wind::PerMeshSettings windSettings;
        std::vector<InstanceData> instanceData;
    };

    struct ShaderInfo {
        glow::SharedTexture2D albedoTex;
        std::vector<std::unique_ptr<VaoInfo>> vaoInfos;
    };

    struct InstanceData {
        tg::vec3 model_col0, model_col1, model_col2, model_col3;
        uint32_t pickID;

        InstanceData(const ECS::Rigid &rigid, uint32_t id);
        static std::vector<glow::ArrayBufferAttribute> attributes();
    };
    std::map<glow::SharedProgram, ShaderInfo> shaderInfos;

    VaoInfo& loadVaoForRendering(const glow::SharedProgram& shader, const glow::SharedTexture2D& albedoTex, const std::string& filePath);
    void updateBuffers();
    void render(MainRenderPass& pass);
};


