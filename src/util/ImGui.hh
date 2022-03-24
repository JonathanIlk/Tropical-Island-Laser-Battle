// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>
#include <imgui/imgui.h>

namespace Util {

static void editColor(const char *name, tg::color4 &color) {
    float color_[4] = {color.r, color.g, color.b, color.a};
    if (ImGui::ColorEdit4(name, color_)) {
        color = {color_[0], color_[1], color_[2], color_[3]};
    }
}

}
