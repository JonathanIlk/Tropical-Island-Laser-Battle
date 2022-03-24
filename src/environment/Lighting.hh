// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg.hh>
#include <glow/std140.hh>

namespace Lighting
{
struct Uniforms {
    // Direction from the ground towards the sun.
    glow::std140vec4 sunDirection = tg::vec4(tg::normalize(tg::vec3(0.34, 0.15, 0.42)));
    glow::std140vec4 sunRadiance;
    glow::std140vec4 ambient;
    glow::std140mat4 lightSpaceViewProj;

    // when we want to support multiple lights, move these into some sort of array
    glow::std140vec4 lightPos;  // xyz: position, w: 1/radius^4
    glow::std140vec4 lightRadiance = tg::vec4(5, 10, 6, 0);

    static float lightRadiusTerm(float r) {
        r *= r;
        return 1 / (r * r);
    }
};

struct Settings
{
    tg::vec3 sunColor = tg::vec3(1.0, 0.25, 0.05);
    float sunIntensity = 0.75;

    tg::vec3 ambientColor = tg::vec3(1.0, 0.9, 0.7);
    float ambientIntensity = 0.45;

    [[nodiscard]] Uniforms getUniforms() const;
    void onGui();
};
}
