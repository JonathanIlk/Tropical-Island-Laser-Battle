// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <glow/fwd.hh>
#include <typed-geometry/tg.hh>
#include "MainRenderPass.hh"
class ScreenSpaceAo
{
    glow::SharedProgram mShaderSsao;
    glow::SharedProgram mShaderSsaoBlurred;
    std::vector<tg::vec3> ssaoKernel;
    std::vector<tg::vec3> ssaoNoise;
    glow::SharedTexture1D ssaoKernelTex;
    glow::SharedTexture2D ssaoNoiseTex;


    glow::SharedFramebuffer mFrameBufferSsaoDepth;
    glow::SharedTextureRectangle mSsaoDepth;

    glow::SharedFramebuffer mFrameBufferSsao;
    glow::SharedTextureRectangle mSsaoTex;

    glow::SharedFramebuffer mFrameBufferSsaoBlurred;

    float ssaoBias = 0.002;
    float ssaoRadius = 0.5;
    bool ssaoEnabled = true;

public:
    glow::SharedTextureRectangle mSsaoBlurredTex;

    void init();
    void resize(int w, int h);

    void renderSsao(MainRenderPass& pass, ECS::ECS &ecs);
    void onGui();
    void clear();
};
