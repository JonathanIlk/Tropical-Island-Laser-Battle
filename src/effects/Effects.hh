// SPDX-License-Identifier: MIT
#pragma once
#include <array>

#include <typed-geometry/tg-lean.hh>
#include <glow/fwd.hh>

#include <fwd.hh>

namespace Effects {

struct ScatterLaser {
    double startTime;
    tg::segment3 seg;
    size_t numFizzle;

    struct Params {
        float radius = .2f, rayWidth = .01f, drift = .1f;
        std::array<float, 4> rayTiming {0.f, .1f, .2f, .3f}, fizzleTiming {.1f, .2f, .8f, 1.f};
        tg::color4 color {0.0, 0.0, 1, 1}, color2{0.22, 0.0, 1, 1};
        unsigned numRays = 5;
        float fizzleDensity = 30.f;  // fizzle particles per meter of length

        void updateUI();
    } params;

    glow::SharedVertexArray rayVAO, fizzleVAO;
};

class System final {
    ECS::ECS &mECS;
    glow::SharedProgram mRayShader, mFizzleShader;
    glow::SharedArrayBuffer mLineSpriteVertices, mPointSpriteVertices;

public:
    System(Game &);

    void renderMain(MainRenderPass &);
    void cleanup(double time);

    void spawnScatterLaser(const tg::segment3 &, const ScatterLaser::Params &);
};

}
