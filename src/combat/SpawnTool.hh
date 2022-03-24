// SPDX-License-Identifier: MIT
#pragma once
#include <Game.hh>

namespace Combat {

class SpawnTool final : public Game::Tool {
    ECS::ECS &mECS;
public:
    SpawnTool(ECS::ECS &ecs) : mECS{ecs} {}
    bool onClick(const tg::ray3 &) override;
    std::optional<ECS::entity> spawnUnit(const tg::ray3&);
    void updateUI() override;
};

}
