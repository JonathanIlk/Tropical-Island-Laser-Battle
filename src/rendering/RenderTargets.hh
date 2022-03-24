// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>
#include <glow/fwd.hh>

struct RenderTargets {
    // intermediate framebuffer with color and depth texture
    glow::SharedFramebuffer mFramebufferScene;
    glow::SharedTextureRectangle mTargetColor;
    glow::SharedTextureRectangle mTargetBloom;
    glow::SharedTextureRectangle mTargetPicking;
    glow::SharedTextureRectangle mTargetDepth;

    glow::SharedFramebuffer mFrameBufferShadow;
    glow::SharedTextureRectangle mShadowTex;

    const int shadowSize = 8192;

    glow::SharedFramebuffer mFramebufferOutline;
    glow::SharedTextureRectangle mTargetOutlineIntermediate;

    glow::SharedFramebuffer mFramebufferReadback;

    tg::color3 mBackgroundColor {1, 1, 1};
    tg::color3 mBloomClearColor = {0.0f, 0.0f, 0.0f};
    tg::color4 mUiClearColor = {0.0f, 0.0f, 0.0f, 0.0f};

    void init();
    void resize(int w, int h);
    void clear();
};
