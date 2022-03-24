// SPDX-License-Identifier: MIT
#include "MainRenderPass.hh"
#include <glow/objects.hh>
#include <ECS.hh>

void MainRenderPass::applyCommons(glow::UsedProgram& shader) {
    shader["uViewProj"] = viewProjMatrix;
    shader["uCamPos"] = cameraPosition;
    shader["uSsaoTex"] = ssaoTex;
    shader["uSunShadowTex"] = shadowTex;
    shader["uClippingPlane"] = clippingPlane;
}

void MainRenderPass::applyTime(glow::UsedProgram& shader) {
    shader["uTime"] = float(snap->worldTime);
}

void MainRenderPass::applyWind(glow::UsedProgram& shader) {
    shader["uWindSettings"] = windUniforms.windSettings;
    shader["uWindTex"] = windUniforms.windTexture;
}
