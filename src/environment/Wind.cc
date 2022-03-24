// SPDX-License-Identifier: MIT
#include "Wind.hh"
#include <glow/objects/Texture2D.hh>
#include <imgui/imgui.h>

#include <rendering/MainRenderPass.hh>

Wind::Uniforms Wind::Settings::getUniforms() {
    auto uniforms = Wind::Uniforms();
    tg::vec2 normalizedDir = tg::normalize(direction);
    uniforms.windSettings = tg::vec4(normalizedDir.x, normalizedDir.y, frequency * 0.01, strength);
    uniforms.windTexture = mWindTexture;
    return uniforms;
}
void Wind::Settings::init() {
    mWindTexture = glow::Texture2D::createFromFile("../data/textures/WindNoise.png", glow::ColorSpace::sRGB);
    mWindTexture->bind().setWrap(GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT);
}
void Wind::Settings::onGui() {
    if (ImGui::TreeNodeEx("Wind", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputFloat2("Direction", &direction.x);
        ImGui::SliderFloat("Frequency", &frequency, 0, 100);
        ImGui::SliderFloat("Strength", &strength, 0, 10);
        ImGui::TreePop();
    }
}
