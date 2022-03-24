// SPDX-License-Identifier: MIT
#pragma once


#include <glad/glad.h>
#include <glow/fwd.hh>
#include <memory>
#include <typed-geometry/types/vec.hh>
#include <vector>

class GaussianBlur
{
    int mKernelSize;
    float mKernelSigma;
    std::vector<float> mKernel;
    glow::SharedTexture1D mKernelTex;

    glow::SharedTextureRectangle mColorTexes[2];
    glow::SharedFramebuffer mFrameBuffersBlur[2];
    glow::SharedProgram mShaderBlur;

    static std::vector<float> calculateBlurKernel(int size, float sigma);
    void updateKernel();

public:

    void init(GLenum internalFormat = GL_R11F_G11F_B10F, int kernelSize = 5, float sigma = 5);
    glow::SharedTextureRectangle& blurTex(const glow::SharedTexture& tex);

    void updateUI();
    void resize(int w, int h);
};
