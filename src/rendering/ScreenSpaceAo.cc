// SPDX-License-Identifier: MIT
#include "ScreenSpaceAo.hh"
#include <random>

#include <glow/common/scoped_gl.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture1D.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow-extras/geometry/FullscreenTriangle.hh>
#include <imgui/imgui.h>

#include <ECS.hh>


void ScreenSpaceAo::renderSsao(MainRenderPass& pass, ECS::ECS &ecs) {
    {
        auto fb = mFrameBufferSsaoDepth->bind();

        GLOW_SCOPED(enable, GL_DEPTH_TEST);
        GLOW_SCOPED(enable, GL_CULL_FACE);
        glClear(GL_DEPTH_BUFFER_BIT);

        ecs.renderSSAO(pass);
    }

    if (ssaoEnabled)
    {
        {
            auto fb = mFrameBufferSsao->bind();

            GLOW_SCOPED(disable, GL_DEPTH_TEST);
            GLOW_SCOPED(disable, GL_CULL_FACE);

            {
                auto shader = mShaderSsao->use();
                shader["uDepthTex"] = mSsaoDepth;
                shader["uProjMat"] = pass.projMatrix;
                shader["uInvProjMat"] = tg::inverse(pass.projMatrix);
                shader["uSsaoKernelTex"] = ssaoKernelTex;
                shader["uNoiseTex"] = ssaoNoiseTex;
                shader["uBias"] = ssaoBias;
                shader["uRadius"] = ssaoRadius;

                glow::geometry::FullscreenTriangle::draw();
            }
        }

        {
            auto fb = mFrameBufferSsaoBlurred->bind();

            GLOW_SCOPED(disable, GL_DEPTH_TEST);
            GLOW_SCOPED(disable, GL_CULL_FACE);

            {
                auto shader = mShaderSsaoBlurred->use();
                shader["ssaoInput"] = mSsaoTex;

                glow::geometry::FullscreenTriangle::draw();
            }
        }
    }
}

void ScreenSpaceAo::init() {
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;

    auto lerp = [&](float a, float b, float f) {
      return a + f * (b - a);
    };

    for (unsigned int i = 0; i < 16; ++i)
    {
        tg::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample  = tg::normalize(sample);
        sample *= randomFloats(generator);
        float scale = (float)i / 16.0f;
        scale   = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    ssaoKernelTex = glow::Texture1D::create(16, GL_RGB16F);
    {
        auto boundTex = ssaoKernelTex->bind();
        boundTex.setFilter(GL_NEAREST, GL_NEAREST);
        boundTex.setData(GL_RGB16F, 16,  ssaoKernel);
    }

    for (unsigned int i = 0; i < 16; i++)
    {
        tg::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    ssaoNoiseTex = glow::Texture2D::create(4, 4, GL_RGBA16F);
    {
        auto boundTex = ssaoNoiseTex->bind();
        boundTex.setFilter(GL_NEAREST, GL_NEAREST);
        boundTex.setWrap(GL_REPEAT, GL_REPEAT);
        boundTex.setData(GL_RGB16F, 4, 4, ssaoNoise);
    }


    mShaderSsao = glow::Program::createFromFiles({"../data/shaders/fullscreen_tri.vsh", "../data/shaders/ssao/ssao.fsh"});
    mShaderSsaoBlurred = glow::Program::createFromFiles({"../data/shaders/fullscreen_tri.vsh", "../data/shaders/ssao/ssaoblur.fsh"});

    mSsaoDepth = glow::TextureRectangle::create(1, 1, GL_DEPTH_COMPONENT32F);

    mFrameBufferSsaoDepth = glow::Framebuffer::create();
    {
        auto boundFb = mFrameBufferSsaoDepth->bind();
        boundFb.attachDepth(mSsaoDepth);
        boundFb.checkComplete();
    }

    mSsaoTex = glow::TextureRectangle::create(1, 1, GL_R16F);
    {
        auto boundTex = mSsaoTex->bind();
        boundTex.setMinFilter(GL_NEAREST);
        boundTex.setMagFilter(GL_NEAREST);
    }

    mFrameBufferSsao = glow::Framebuffer::create();
    {
        auto boundFb = mFrameBufferSsao->bind();
        boundFb.attachColor("ao", mSsaoTex);
        boundFb.checkComplete();
    }

    mSsaoBlurredTex = glow::TextureRectangle::create(1, 1, GL_R16F);
    {
        auto boundTex = mSsaoBlurredTex->bind();
        boundTex.setMinFilter(GL_NEAREST);
        boundTex.setMagFilter(GL_NEAREST);
    }

    mFrameBufferSsaoBlurred = glow::Framebuffer::create();
    {
        auto boundFb = mFrameBufferSsaoBlurred->bind();
        boundFb.attachColor("ao", mSsaoBlurredTex);

        boundFb.checkComplete();
    }

}

void ScreenSpaceAo::resize(int w, int h) {
    for (auto &t : {
        mSsaoDepth, mSsaoTex, mSsaoBlurredTex
    }) {
        t->bind().resize(w, h);
    }
}

void ScreenSpaceAo::onGui() {
    if (ImGui::TreeNodeEx("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enabled", &ssaoEnabled);
        ImGui::InputFloat("Radius", &ssaoRadius, 0, 1);
        ImGui::InputFloat("Bias", &ssaoBias, 0, 1);
        ImGui::TreePop();
    }
}

void ScreenSpaceAo::clear() {
    const float clearCol = 1;
    mSsaoTex->clear(GL_RGB, GL_FLOAT, &clearCol);
    mSsaoBlurredTex->clear(GL_RGB, GL_FLOAT, &clearCol);
}
