// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg.hh>
#include <glow/fwd.hh>

namespace Wind {

    struct Uniforms {
        tg::vec4 windSettings;
        glow::SharedTexture2D windTexture;
    };

    struct PerMeshSettings
    {
        float startFromHeight = 0;
        float objectHeight;
    };

    struct Settings
    {
        Uniforms getUniforms();
        glow::SharedTexture2D mWindTexture;

        void init();

        tg::vec2 direction = tg::vec2(1, 1); // xy = direction (x,z worldSpace)
        float frequency = 4.3;
        float strength = 3.0f;
        void onGui();
    };


}
