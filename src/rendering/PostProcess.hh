// SPDX-License-Identifier: MIT
#pragma once
#include <glow/fwd.hh>
#include "GaussianBlur.hh"
#include "MainRenderPass.hh"

struct RenderTargets;

class PostProcess {
private:
    glow::SharedProgram mShaderOutput;
    GaussianBlur mGaussianBlur;
    GaussianBlur mBloomBlur;

    float mStartDoFDistance = 10;
    float mFinalDoFDistance = 40;
    bool mEnableTonemap = true;
    bool mEnableSRGBCurve = true;
    bool mEnableDoF = true;
    bool mEnableBloom = true;
    tg::vec2 mVignetteBorder {.8f, .8f};
    tg::color4 mVignettePaused {0, 0, 0, .5f};
    tg::color4 mVignetteWarning {1, 0, 0, .5f};
    float mVignetteTime = 0.f;
    tg::color4 mVignettePulse;

public:
    float mFocusDistance = 20;

    void init();
    void render(RenderTargets &, const tg::mat4& projMat, float timePassed, bool paused);
    void updateUI();
    void resize(int w, int h);

    void flashWarning(float fadeTime);
};
