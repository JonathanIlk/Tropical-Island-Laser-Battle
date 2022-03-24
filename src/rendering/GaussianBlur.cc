// SPDX-License-Identifier: MIT
#include "GaussianBlur.hh"
#include <imgui/imgui.h>
#include <glow-extras/geometry/FullscreenTriangle.hh>
#include <glow/common/scoped_gl.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture1D.hh>
#include <glow/objects/TextureRectangle.hh>


void GaussianBlur::init(GLenum internalFormat, int kernelSize, float sigma) {
    mKernelSize = kernelSize;
    mKernelSigma = sigma;
    updateKernel();

    mShaderBlur = glow::Program::createFromFiles({"../data/shaders/fullscreen_tri.vsh", "../data/shaders/postprocess/gaussian_blur.fsh"});

    for(int i = 0; i < 2; i++) {
        mColorTexes[i] = glow::TextureRectangle::create(1, 1, internalFormat);
        {
            auto boundTex = mColorTexes[i]->bind();
            boundTex.setFilter(GL_LINEAR, GL_LINEAR);
            boundTex.setWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }

        mFrameBuffersBlur[i] = glow::Framebuffer::create();
        {
            auto boundFb = mFrameBuffersBlur[i]->bind();
            boundFb.attachColor("color", mColorTexes[i]);
            boundFb.checkComplete();
        }
    }

}

void GaussianBlur::updateKernel() {
    mKernel = calculateBlurKernel(mKernelSize, mKernelSigma);

    mKernelTex = glow::Texture1D::create(mKernelSize, GL_R16F);
    {
        auto boundTex = mKernelTex->bind();
        boundTex.setFilter(GL_NEAREST, GL_NEAREST);
        boundTex.setData(GL_R16F, mKernelSize, mKernel);
    }
}

std::vector<float> GaussianBlur::calculateBlurKernel(int size, float sigma) {
    std::vector<float> kernel(size);
    float sum = 0;
    for(int i = 0; i < size; i++) {
        kernel[i] = std::exp(-0.5 * pow(i / sigma, 2.0));
        sum += kernel[i];
    }
    kernel[0] /= 2;  // shader samples the center pixel twice
    sum -= kernel[0];

    // normalize: our kernel texture contains only one side of the bell curve, so it
    // needs to sum up to 0.5
    float factor = .5f / sum;
    for (float &v : kernel) {v *= factor;}

    return kernel;
}

glow::SharedTextureRectangle& GaussianBlur::blurTex(const glow::SharedTexture& tex) {
    GLOW_SCOPED(disable, GL_DEPTH_TEST);
    GLOW_SCOPED(disable, GL_CULL_FACE);
    auto shader = mShaderBlur->use();
    shader["uKernelTex"] = mKernelTex;

    {
        auto fb = mFrameBuffersBlur[0]->bind();
        shader["uInputTex"] = tex;
        shader["uSampleDirection"] = tg::ivec2(1, 0);
        glow::geometry::FullscreenTriangle::draw();
    } {
        auto fb = mFrameBuffersBlur[1]->bind();
        shader["uInputTex"] = mColorTexes[0];
        shader["uSampleDirection"] = tg::ivec2(0, 1);
        glow::geometry::FullscreenTriangle::draw();
    }
    return mColorTexes[1];
}

void GaussianBlur::updateUI() {
    bool update = false;
    update |= ImGui::SliderInt("Kernel Size", &mKernelSize, 1, 30);
    update |= ImGui::SliderFloat("StdDev", &mKernelSigma, 1, 10);
    if (update) {updateKernel();}
}

void GaussianBlur::resize(int w, int h) {
    for (auto &t : mColorTexes) {
        t->bind().resize(w, h);
    }
}
