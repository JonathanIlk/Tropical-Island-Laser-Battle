// SPDX-License-Identifier: MIT
#pragma once
#include <optional>

#include <polymesh/Mesh.hh>
#include <glow/fwd.hh>

#include <Game.hh>

namespace NavMesh {

class PathfinderTool final : public Game::Tool {
    ECS::ECS &mECS;

    glow::SharedVertexArray mVaoMarker;
    glow::SharedProgram mShader;
    glow::SharedArrayBuffer mABPath;
    glow::SharedVertexArray mVaoPath;
    size_t mPathEnd = 0, mVaoEnd = 0;

    struct SelectedPoint {
        tg::pos3 pos;
        ECS::entity navmesh;
        pm::face_index face;
    };
    std::array<std::optional<SelectedPoint>, 2> mPoints {};
    size_t mCurPoint = 0;
    int mNSteps = 3;
    float mRadius = .5f, mHeight = 1.8f;

    bool updatePath();

public:
    PathfinderTool(Game &game);
    bool onClick(const tg::ray3 &ray) override;
    void updateUI() override;
    void renderMain(MainRenderPass &) override;
};

}
