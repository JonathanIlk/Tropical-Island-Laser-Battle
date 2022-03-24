// SPDX-License-Identifier: MIT
#pragma once
#include <glow/fwd.hh>
#include <typed-geometry/tg-lean.hh>

#include <fwd.hh>
#include <environment/Wind.hh>

struct MainRenderPass {
    ECS::Snapshot *snap;
    double wallTime;

    tg::mat4x3 viewMatrix;
    tg::mat4 projMatrix;
    tg::mat4 viewProjMatrix;
    tg::vec4 clippingPlane;
    tg::pos3 cameraPosition;
    tg::isize2 viewPortSize;

    glow::SharedTextureRectangle shadowTex;
    glow::SharedTextureRectangle ssaoTex;
    glow::SharedUniformBuffer lightingUniforms;
    Wind::Uniforms windUniforms;

    void applyCommons(glow::UsedProgram& shader);
    void applyTime(glow::UsedProgram& shader);
    void applyWind(glow::UsedProgram& shader);
};
