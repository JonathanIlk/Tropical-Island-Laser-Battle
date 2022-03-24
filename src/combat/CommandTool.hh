// SPDX-License-Identifier: MIT
#pragma once
#include <memory>
#include <optional>

#include <polymesh/Mesh.hh>

#include <Game.hh>

namespace Combat {

class CommandTool final : public Game::Tool {
    ECS::ECS &mECS;
    std::unique_ptr<Game::Tool> &mActiveTool;
    std::optional<std::tuple<ECS::entity, pm::face_index, tg::pos3>> mDestination;
    std::optional<tg::quat> mOrient;

    glow::SharedVertexArray mVaoMarker;
    glow::SharedVertexArray mVaoFan;
    glow::SharedProgram mSimpleShader;

    bool findDestination(const tg::ray3 &);
    bool navigate(const tg::ray3 &);
public:
    CommandTool(Game &, tg::angle32 viewAngle, float innerRadius, float outerRadius);
    bool onClick(const tg::ray3 &) override;
    void processInput(const glow::input::InputState &, const tg::ray3 &) override;
    void renderMain(MainRenderPass &) override;
    void updateUI() override;
};

}
