// SPDX-License-Identifier: MIT
#pragma once

#include <Mesh3D.hh>
#include <animation/Animator.hh>
#include <glow-extras/assimp/MeshData.hh>
#include <fwd.hh>
#include <glow/fwd.hh>
#include <memory>

#define MAX_BONES 8

namespace RiggedMesh
{

class Data
{
    void addAnimationFromData(glow::assimp::SharedMeshData& sharedPtr);

public:
    glow::SharedVertexArray mVao;
    std::string rootBoneName;
    std::map<std::string, glow::assimp::BoneData> bones;
    std::map<std::string, std::shared_ptr<RiggedAnimation>> animations;

    void loadMesh(std::string const& filename, const std::string& loadWithAnimName);
    void addAnimation(std::string const& filename, const std::string& loadWithAnimName);
};

class Instance
{
public:
    Instance() = default;
    explicit Instance(Data* data, const std::string& startAnimName);
    Data* meshData;
    std::shared_ptr<RiggedAnimator> animator;
};

class System
{
    Game& mGame;

public:
    glow::SharedTexture2D mTexAlbedo;
    glow::SharedProgram mShader;
    explicit System(Game& game);
    void renderMain(MainRenderPass&);
    void renderInstance(RiggedMesh::Instance&, MainRenderPass&, tg::mat4x3) const;
};
}
