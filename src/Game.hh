// SPDX-License-Identifier: MIT
#pragma once
#include <animation/KeyFrame.hh>
#include <animation/rigged/RiggedMesh.hh>
#include <glow-extras/glfw/GlfwApp.hh>
#include <glow/fwd.hh>
#include <rendering/ScreenSpaceAo.hh>

#include "Camera.hh"
#include "ECS.hh"
#include "environment/Lighting.hh"
#include "environment/Wind.hh"
#include "fwd.hh"
#include "rendering/PostProcess.hh"
#include "rendering/RenderTargets.hh"
#include "rendering/SharedResources.hh"

class Game : public glow::glfw::GlfwApp
{
    int mSceneSeed = 42;

    bool mPaused = false;
    double mTimeDelta = 0.;

    bool mDevMode = true;

    glow::SharedTexture2D mControls;

public:
    class Tool {
    public:
        virtual ~Tool() {}
        virtual bool onClick(const tg::ray3 &ray) {return true;}
        virtual void processInput(const glow::input::InputState &, const tg::ray3 &mouseWorldRay) {}
        virtual void updateUI() = 0;
        virtual void renderMain(MainRenderPass &) {}
    };
    std::unique_ptr<Tool> mActiveTool;
    bool mShowWireframe = false;
    bool mCaptureMouseOnMouselook = true;
    SharedResources mSharedResources;
    RenderTargets mRenderTargets;
    Camera mCamera;
    ScreenSpaceAo mSsao;
    PostProcess mPostProcess;
    Lighting::Settings mLightingSettings;
    Wind::Settings mWindSettings;
    glow::SharedUniformBuffer mLightingUB;
    static constexpr size_t STAT_FRAMES = 256;
    struct FrameStats {
        float time = 0.0f;
        unsigned updates = 0;
    } mFrameStats[STAT_FRAMES];
    size_t mCurFrameStat = 0;
    FrameStats &currentFrameStat() {return mFrameStats[mCurFrameStat];}

    Game();
    ~Game();

    // === events
    void init() override;                       // called once after OpenGL is set up
    void onFrameStart() override;               // called at the start of a frame
    void update(float elapsedSeconds) override; // called in 60 Hz fixed timestep
    void render(float elapsedSeconds) override; // called once per frame (variable timestep)
    void onGui() override;                      // called once per frame to set up UI
    void onResize(int w, int h) override; // called when window is resized

    tg::dir3 mouseWorldDirection();
    void updateCamera(float elapsedSeconds);
    void defaultEditorWindow();
    void startScreen();
    void terrainScene(const ECS::Rigid &pos, bool startIntro);

    void pause(bool paused = true);

    // read the picking buffer at the specified pixel coordinate
    ECS::entity readPickingBuffer(int x, int y);

    ECS::ECS mECS;
    ECS::Snapshot mSnaps[2];
    size_t mCurSnap = 0;
    ECS::Snapshot &simSnap() {return mSnaps[mCurSnap];}

    // should probably become a component in the future
    tg::pos3 mLightPos;
    tg::vec3 mLightRadiance;
    float mLightRadius;
};
