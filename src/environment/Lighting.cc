// SPDX-License-Identifier: MIT
#include "Lighting.hh"
#include <imgui/imgui.h>

using namespace Lighting;


Uniforms Settings::getUniforms() const {
    auto uniforms = Lighting::Uniforms();
    uniforms.ambient = tg::vec4(ambientColor * ambientIntensity);
    uniforms.sunRadiance = tg::vec4(sunColor * sunIntensity);

    return uniforms;
}
void Settings::onGui() {
    if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Sun Color", &sunColor.x);
        ImGui::SliderFloat("Sun Intensity", &sunIntensity, 0, 1);
        ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
        ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0, 1);
        ImGui::TreePop();
    }
}
