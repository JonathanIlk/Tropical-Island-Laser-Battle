// SPDX-License-Identifier: MIT
#include "PostProcess.hh"
#include <glow/objects/Program.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/common/scoped_gl.hh>
#include <glow-extras/geometry/FullscreenTriangle.hh>

#include "MainRenderPass.hh"
#include "RenderTargets.hh"
#include <util/ImGui.hh>

void PostProcess::init() {
    mShaderOutput = glow::Program::createFromFiles({"../data/shaders/fullscreen_tri.vsh", "../data/shaders/output.fsh"});
    mGaussianBlur.init();
    mBloomBlur.init();
    glow::geometry::FullscreenTriangle::init();
}

void PostProcess::render(RenderTargets &rt, const tg::mat4& projMat, float timePassed, bool paused) {
    auto blurredTex = mEnableDoF
        ? mGaussianBlur.blurTex(rt.mTargetColor)
        : rt.mTargetColor;

    auto& bloomTex = mEnableBloom ? mBloomBlur.blurTex(rt.mTargetBloom) : rt.mTargetBloom;

    auto vignetteColor = paused ? mVignettePaused : tg::color4(0, 0, 0, 0);
    mVignetteTime = std::max(mVignetteTime - timePassed, 0.f);
    vignetteColor = tg::lerp(vignetteColor, mVignettePulse, mVignetteTime);

    GLOW_SCOPED(disable, GL_DEPTH_TEST);
    GLOW_SCOPED(disable, GL_CULL_FACE);

    {
        auto shader = mShaderOutput->use();
        shader["uTexColor"] = rt.mTargetColor;
        shader["uStartDoFDistance"] = mStartDoFDistance;
        shader["uFinalDoFDistance"] = mFinalDoFDistance;
        shader["uInvProjMat"] = tg::inverse(projMat);
        shader["uBlurredTexColor"] = blurredTex;
        shader["uTexDepth"] = rt.mTargetDepth;
        shader["uTexBloom"] = bloomTex;
        shader["uEnableTonemap"] = mEnableTonemap;
        shader["uEnableGamma"] = mEnableSRGBCurve;
        shader["uWsFocusDistance"] = mFocusDistance;
        shader["uVignetteBorder"] = mVignetteBorder;
        shader["uVignetteColor"] = vignetteColor;

        glow::geometry::FullscreenTriangle::draw();
    }
}

void PostProcess::updateUI() {
    ImGui::Checkbox("Enable Tonemapping", &mEnableTonemap);
    ImGui::Checkbox("Enable sRGB Output", &mEnableSRGBCurve);
    if (ImGui::TreeNode("Vignette")) {
        float vignetteBorder[2] {mVignetteBorder.x, mVignetteBorder.y};
        if (ImGui::SliderFloat2("border", vignetteBorder, 0.f, 1.f)) {
            mVignetteBorder = {vignetteBorder[0], vignetteBorder[1]};
        }
        Util::editColor("paused color", mVignettePaused);
        Util::editColor("warning color", mVignetteWarning);
    }
    if (ImGui::TreeNodeEx("Depth of Field", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * .2f);
        ImGui::Checkbox("Enable", &mEnableDoF); ImGui::SameLine();
        ImGui::InputFloat("Near", &mStartDoFDistance); ImGui::SameLine();
        ImGui::InputFloat("Far", &mFinalDoFDistance);
        ImGui::PopItemWidth();
        ImGui::InputFloat("FocusDistance", &mFocusDistance);
        mGaussianBlur.updateUI();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable", &mEnableBloom);
        mBloomBlur.updateUI();
        ImGui::TreePop();
    }
}

void PostProcess::resize(int w, int h) {
    mGaussianBlur.resize(w, h);
    mBloomBlur.resize(w, h);
}

void PostProcess::flashWarning(float fadeTime) {
    mVignettePulse = mVignetteWarning;
    mVignetteTime = fadeTime;
}
