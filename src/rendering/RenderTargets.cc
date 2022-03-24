// SPDX-License-Identifier: MIT
#include "RenderTargets.hh"

#include <glow-extras/colors/color.hh>
#include <glow/glow.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/TextureRectangle.hh>
#include <typed-geometry/tg.hh>

void RenderTargets::init() {
        // size is 1x1 for now and is changed onResize
        mTargetColor = glow::TextureRectangle::create(1, 1, GL_R11F_G11F_B10F);
        mTargetBloom = glow::TextureRectangle::create(1, 1, GL_R11F_G11F_B10F);
        mTargetPicking = glow::TextureRectangle::create(1, 1, GL_R32UI);
        mTargetDepth = glow::TextureRectangle::create(1, 1, GL_DEPTH_COMPONENT32F);
        mTargetOutlineIntermediate = glow::TextureRectangle::create(1, 1, GL_RGBA8);

        // create framebuffer for HDR scene
        mFramebufferScene = glow::Framebuffer::create();
        {
            auto boundFb = mFramebufferScene->bind();
            boundFb.attachColor("fColor", mTargetColor);
            boundFb.attachColor("fEmission", mTargetBloom);
            boundFb.attachColor("fPickID", mTargetPicking);
            boundFb.attachDepth(mTargetDepth);
            boundFb.checkComplete();
        }

        {
            mShadowTex = glow::TextureRectangle::create(shadowSize, shadowSize, GL_DEPTH_COMPONENT32F);
            auto boundTex = mShadowTex->bind();
            boundTex.setCompareMode(GL_COMPARE_REF_TO_TEXTURE);
            boundTex.setCompareFunc(GL_LEQUAL);
            boundTex.setAnisotropicFiltering(1);
            boundTex.setWrap(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
            boundTex.setBorderColor(tg::color(1.f, 1.f, 1.f, 1.f));
        }

        mFrameBufferShadow = glow::Framebuffer::create();
        {
            auto boundFb = mFrameBufferShadow->bind();
            boundFb.attachDepth(mShadowTex);
            boundFb.checkComplete();
        }

        // create framebuffer for the outline (depth tested)
        mFramebufferOutline = glow::Framebuffer::create("fColor", mTargetOutlineIntermediate, mTargetDepth);

        // create framebuffer specifically for reading back the picking buffer
        mFramebufferReadback = glow::Framebuffer::create("fPickID", mTargetPicking);
}

void RenderTargets::resize(int w, int h) {
    for (auto &t : {
        mTargetColor, mTargetBloom,  mTargetPicking, mTargetDepth, mTargetOutlineIntermediate
    }) {
        t->bind().resize(w, h);
    }
}

void RenderTargets::clear() {
    // clear scene targets - depth to 1 (= far plane), scene to background color, picking to -1u (= invalid)
    mTargetColor->clear(GL_RGB, GL_FLOAT, tg::data_ptr(mBackgroundColor));
    mTargetBloom->clear(GL_RGB, GL_FLOAT, tg::data_ptr(mBloomClearColor));
    float const depthVal = 1.f;
    mTargetDepth->clear(GL_DEPTH_COMPONENT, GL_FLOAT, &depthVal);
    uint32_t const pickingVal = uint32_t(-1);
    mTargetPicking->clear(GL_RED_INTEGER, GL_UNSIGNED_INT, &pickingVal);
}
