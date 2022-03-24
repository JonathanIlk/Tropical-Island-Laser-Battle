// SPDX-License-Identifier: MIT
#include "Game.hh"
#include <cinttypes>
#include <limits>

#include <typed-geometry/tg.hh> // math library
#include <typed-geometry/feature/std-interop.hh> // support for writing tg objects to streams
#include <typed-geometry/functions/matrix/inverse.hh>

#include <glow/common/log.hh>
#include <glow/common/scoped_gl.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/UniformBuffer.hh>
#include <glow/objects/VertexArray.hh>

#include <glow-extras/debugging/imgui-util.hh>
#include <glow-extras/geometry/Quad.hh>

#include <GLFW/glfw3.h> // window/input framework

#include <imgui/imgui.h> // UI framework

#include "Util.hh"
#include <animation/AnimatorManager.hh>
#include <startsequence/StartSequence.hh>
#include <animation/rigged/RiggedMesh.hh>
#include <terrain/SkyBox.hh>
#include <terrain/Terrain.hh>
#include <terrain/Water.hh>
#include <ui/SpriteRenderer.hh>
#include "ECS/Join.hh"
#include "SimpleMesh.hh"
#include "advanced/Utility.hh"
#include "combat/Combat.hh"
#include "combat/CommandTool.hh"
#include "combat/SpawnTool.hh"
#include "demo/Demo.hh"
#include "effects/Effects.hh"
#include "environment/Lighting.hh"
#include "environment/Parrot.hh"
#include "navmesh/NavMesh.hh"
#include "navmesh/PathfinderTool.hh"
#include "obstacles/Obstacle.hh"
#include "obstacles/WorldFluff.hh"
#include "rendering/MainRenderPass.hh"
#include "rendering/MeshViz.hh"

Game::Game() : GlfwApp(Gui::ImGui) {}
Game::~Game() {}

void Game::init()
{
    // enable vertical synchronization to synchronize rendering to monitor refresh rate
    setVSync(false);

    // disable built-in camera
    setUseDefaultCamera(false);

    setEnableDebugOverlay(false);

    // set the window resolution
    setWindowWidth(1600);
    setWindowHeight(900);

    // IMPORTANT: call to base class
    GlfwApp::init();

    // set the GUI color theme
    glow::debugging::applyGlowImguiTheme(true);

    // set window title
    setTitle("Game Development 2021");

    mRenderTargets.init();
    // make sure to build the shaders after creating the render targets.
    // It defies any reason, but some shaders behave weirdly if linked before the
    // framebuffers. Probably a bug in GLOW somewhere … (FIXME for after the
    // practical)
    mSharedResources.init();
    mPostProcess.init();
    mLightingUB = glow::UniformBuffer::create();
    mWindSettings.init();
    mECS.init(*this);
    mSsao.init();
    mControls = glow::Texture2D::createFromFile("../data/textures/controls.png", glow::ColorSpace::Linear);

    if(!mDevMode) {
        toggleFullscreen();
    }
}

void Game::onFrameStart()
{
    mCurFrameStat = (mCurFrameStat + 1) % STAT_FRAMES;
    currentFrameStat() = {};
}

void Game::pause(bool paused) {
    if (paused == mPaused) {return;}
    mPaused = paused;
    if (paused) {
        glow::info() << "Za warudo. Toki wa tomatta.";
    } else {
        mTimeDelta = getCurrentTimeD() - simSnap().worldTime;
        glow::info() << "Toki wa ugokidasu.";
    }
}

// update game in 60 Hz fixed timestep
void Game::update(float elapsedSeconds)
{
    if (mPaused) {return;}
    currentFrameStat().updates += 1;

    auto &prev = simSnap();
    mCurSnap = 1 - mCurSnap;
    auto &next = simSnap();
    next.worldTime = getCurrentTimeD() - mTimeDelta;
    mECS.simSnap = &next;
    mECS.extrapolateUpdate(prev, next);
    mECS.fixedUpdate();
    mECS.cleanup(next.worldTime);
}


// render game variable timestep
void Game::render(float elapsedSeconds)
{
    currentFrameStat().time = elapsedSeconds;
    Lighting::Uniforms lighting = mLightingSettings.getUniforms();

    // camera update and animations here because fixed update rates would cause visible stutter with an unlocked framerate
    // however there are ways around this as well - you could always update in a fixed timestep and interpolate between current and previous state
    updateCamera(elapsedSeconds);

    auto wallTime = getRenderTimeD();
    auto worldTime = mPaused ? simSnap().worldTime : wallTime - mTimeDelta;

    // let a pointlight rotate around the objects
    auto angle = tg::angle32(100._deg * worldTime);
    auto r = 3;
    mLightPos = tg::pos3(tg::cos(angle) * r, 4, tg::sin(angle) * r);

    if (!mPaused) {
        AnimatorManager::updateAllAnimators(elapsedSeconds);
    }
    mECS.startSequenceSys->applyAnimations(tg::pos3(tg::vec4(lighting.sunDirection)) * 10000.0f);

    // picking
    auto &in = input();
    if (mActiveTool) {
        mActiveTool->processInput(
            input(), {mCamera.mPos, mouseWorldDirection()}
        );
    }
    if (!ImGui::GetIO().WantCaptureMouse && in.isMouseButtonPressed(GLFW_MOUSE_BUTTON_1))
    {
        if (mActiveTool) {
            if (!mActiveTool->onClick(tg::ray3(mCamera.mPos, mouseWorldDirection()))) {
                mPostProcess.flashWarning(.5f);
            }
        } else {
            auto const mousePos = input().getMousePosition();
            mECS.selectedEntity = readPickingBuffer(int(mousePos.x), getWindowHeight() - int(mousePos.y));
            glow::info() << "selected entity " << mECS.selectedEntity;

            if (!mDevMode) {
                auto join = ECS::Join(mECS.humanoids, mECS.mobileUnits);
                auto iter = join.find(mECS.selectedEntity);
                if (iter != join.end()) {
                    auto [hum, mob, id] = *iter;
                    mActiveTool = std::make_unique<Combat::CommandTool>(
                        *this, tg::acos(hum.attackCos), mob.radius, hum.attackRange
                    );
                } else {
                    glow::info() << "No default tool for entity";
                    mECS.selectedEntity = ECS::INVALID;
                }
            }
        }
    }
    if (in.isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (mActiveTool || mECS.selectedEntity != ECS::INVALID) {
            mActiveTool = nullptr;
            mECS.selectedEntity = ECS::INVALID;
        } else if (mDevMode) {
            mDevMode = false;
        } else {
            pause(true);
            mDevMode = true;
            mPostProcess.flashWarning(.5f);
        }
    }
    if (in.isKeyPressed(GLFW_KEY_SPACE)) {
        pause(!mPaused);
    }
    if (in.isKeyPressed(GLFW_KEY_ENTER) && mECS.nextEntity == 0) {
        terrainScene({
            {0, 0, 0}, tg::quat::from_axis_angle(tg::dir3(0, 1, 0), 135_deg)
        }, true);
    }

    MainRenderPass pass;
    pass.snap = &mSnaps[1 - mCurSnap];
    pass.wallTime = wallTime;
    pass.snap->worldTime = worldTime;
    mECS.extrapolateRender(*mECS.simSnap, *pass.snap);
    if (mPaused) {mECS.combatSys->prepareUI(*pass.snap);}

    pass.cameraPosition = mCamera.mPos;
    tg::mat4 const proj = mCamera.projectionMatrix();
    tg::mat4x3 const view = mCamera.viewMatrix();
    pass.projMatrix = proj;
    pass.viewMatrix = view;
    tg::mat4 view4x4(view);
    view4x4[3][3] = 1.0;
    pass.viewProjMatrix = proj * view4x4;
    pass.shadowTex = mRenderTargets.mShadowTex;
    pass.ssaoTex = mSsao.mSsaoBlurredTex;

    lighting.lightPos = tg::vec4(mLightPos, Lighting::Uniforms::lightRadiusTerm(50));

    pass.windUniforms = mWindSettings.getUniforms();
    mSsao.clear();

    {
        auto fb = mRenderTargets.mFrameBufferShadow->bind();
        GLOW_SCOPED(enable, GL_DEPTH_TEST);
        GLOW_SCOPED(enable, GL_CULL_FACE);
        glClear(GL_DEPTH_BUFFER_BIT);

        MainRenderPass shadowPass = pass;
        shadowPass.viewPortSize = mRenderTargets.mShadowTex->getSize();

        tg::vec4 directionToSun4(lighting.sunDirection);
        tg::vec3 directionToSun(directionToSun4.x, directionToSun4.y, directionToSun4.z);

        auto shadowRadius = 250.f;
        tg::mat4 lightProj = tg::orthographic(-shadowRadius, shadowRadius, -shadowRadius, shadowRadius, 50.0f, 250.0f);
        tg::pos3 shadowMapCameraPos = tg::pos3(pass.cameraPosition) + directionToSun * 100;
        tg::mat4 lightView = tg::look_at_opengl(shadowMapCameraPos, pass.cameraPosition, tg::vec3::unit_y);

        shadowPass.projMatrix = lightProj;
        shadowPass.viewMatrix = tg::mat4x3::from_rows(lightView.row(0), lightView.row(1), lightView.row(2));
        tg::mat4 shadowVPMat = shadowPass.projMatrix * lightView;
        shadowPass.viewProjMatrix = shadowVPMat;
        shadowPass.cameraPosition = shadowMapCameraPos;

        shadowVPMat[3] += tg::vec4(1, 1, 1, 0);
        shadowVPMat.set_row(3, shadowVPMat.row(3) * 2);
        lighting.lightSpaceViewProj = shadowVPMat;
        mLightingUB->bind().setData(lighting, GL_DYNAMIC_DRAW);
        pass.lightingUniforms = mLightingUB;

        mECS.renderShadow(shadowPass);
    }

    for (auto &&tup : ECS::Join(pass.snap->rigids, mECS.terrains, mECS.waters)) {
        auto &[rigid, terrain, water, id] = tup;
        water.clearFramebuffers(mRenderTargets.mBackgroundColor);

        GLOW_SCOPED(enable, GL_CLIP_DISTANCE0);
        GLOW_SCOPED(enable, GL_DEPTH_TEST);
        GLOW_SCOPED(enable, GL_CULL_FACE);
        GLOW_SCOPED(polygonMode, mShowWireframe ? GL_LINE : GL_FILL);
        auto tmat = rigid.transform_mat();
        auto normal = tg::normalize(tg::vec3(tmat * tg::vec4(0, 1, 0, 0)));
        auto point = tg::vec3(tmat * tg::vec4(0, terrain.waterLevel, 0, 1));
        tg::vec4 surfacePlane = tg::vec4(normal, -dot(point, normal));
        tg::vec4 clippingSlack = tg::vec4(0, 0, 0, water.waveHeight);

        {
            auto fb = water.fbRefract->bind();
            pass.viewPortSize = water.refractColor->getSize();

            GLOW_SCOPED(enable, GL_BLEND);
            GLOW_SCOPED(blendFunc, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Only render under water scenery
            pass.clippingPlane = -surfacePlane + clippingSlack;

            mECS.renderReflectRefract(pass);
        }

        {
            auto fb = water.fbReflect->bind();
            pass.viewPortSize = water.reflectColor->getSize();

            MainRenderPass reflectionPass = pass;

            // Only render above water scenery
            reflectionPass.clippingPlane = surfacePlane + clippingSlack;
            float cameraDist = tg::dot(normal, pass.cameraPosition) + surfacePlane.w;
            tg::mat4 reflectionMatrix = Water::System::calculateReflectionMatrix(surfacePlane);

            reflectionPass.viewMatrix = tg::mat4x3(view4x4 * reflectionMatrix);
            reflectionPass.viewProjMatrix = reflectionPass.viewProjMatrix * reflectionMatrix;
            reflectionPass.cameraPosition = reflectionMatrix * pass.cameraPosition;

            mECS.renderReflectRefract(reflectionPass);
        }
    }
    pass.clippingPlane = tg::vec4();

    mSsao.renderSsao(pass, mECS);

    mRenderTargets.clear();

    // render everything into the HDR scene framebuffer
    {
        auto fb = mRenderTargets.mFramebufferScene->bind();
        // glViewport is automatically set by framebuffer
        pass.viewPortSize = getWindowSize();

        GLOW_SCOPED(enable, GL_DEPTH_TEST);
        GLOW_SCOPED(enable, GL_CULL_FACE);
        GLOW_SCOPED(enable, GL_BLEND);
        GLOW_SCOPED(blendFunc, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLOW_SCOPED(polygonMode, mShowWireframe ? GL_LINE : GL_FILL);

        mECS.renderMain(pass);
        mECS.renderTransparent(pass);
        if (mActiveTool) {mActiveTool->renderMain(pass);}
        if (mPaused) {mECS.combatSys->renderUI(pass);}
    }

    mPostProcess.render(mRenderTargets, pass.projMatrix, elapsedSeconds, mPaused);

    {
        GLOW_SCOPED(enable, GL_BLEND);
        GLOW_SCOPED(blendFunc, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLOW_SCOPED(polygonMode, mShowWireframe ? GL_LINE : GL_FILL);

        mECS.renderUi();
        if (mECS.nextEntity == 0) {
            startScreen();
        }
    }
}

void Game::startScreen() {
    auto quadrantSize = tg::size2(getWindowSize()) / 2;
    auto sh = mSharedResources.sprite->use();
    auto vao = mSharedResources.spriteQuad->bind();
    sh["uAlpha"] = 1.f;
    tg::mat3x2 mat;

    auto logoSize = .5f * tg::size2(mSharedResources.logo->getSize()) / quadrantSize;
    mat[0][0] = logoSize.width;
    mat[1][1] = logoSize.height;
    mat[2] = {-.5f * logoSize.width, logoSize.height};
    sh["uImage"] = mSharedResources.logo;
    sh["uTransform"] = mat;
    vao.draw();
    auto controlsSize = tg::size2(mControls->getSize()) / quadrantSize;
    mat[0][0] = controlsSize.width;
    mat[1][1] = controlsSize.height;
    mat[2] = {-.5f * controlsSize.width, 0};
    sh["uImage"] = mControls;
    sh["uTransform"] = mat;
    vao.draw();
}

tg::dir3 Game::mouseWorldDirection() {
    auto pos = tg::vec2(input().getMousePosition());
    const auto size = getWindowSize();
    auto ndc = tg::vec2(-1 + 2 * pos.x / size.width, 1 - 2 * pos.y / size.height);
    return mCamera.ndc2dir(ndc);
}

void Game::terrainScene(const ECS::Rigid &pos, bool startIntro) {
    ECS::entity ent = mECS.newEntity();
    glow::info() << "creating terrain " << ent;
    mECS.editables.emplace(ent, &*mECS.terrainSys);
    auto &wo = mECS.staticRigids.emplace(ent, pos).first->second;
    std::mt19937 rng(mSceneSeed);
    auto &terr = mECS.terrains.emplace(ent, Terrain::Instance(rng)).first->second;
    mECS.terrainRenderings.emplace(ent, Terrain::Rendering(terr));
    mECS.waters.emplace(ent, Water::Instance(terr, getWindowSize()));
    mECS.skyBoxes.emplace(ent, SkyBox::Instance(terr));
    auto &nav = mECS.navMeshes.emplace(ent, NavMesh::Instance(wo, terr)).first->second;
    mECS.obstacleSys->spawnObstacles(wo, terr, rng);
    mECS.worldFluffSys->spawnFluff(wo, terr, rng);
    for (auto i : Util::IntRange(10)) {
        mECS.combatSys->spawnSquad(nav, 5, 30.f, rng);
    }
    if (startIntro) {
        mECS.startSequenceSys->startSequence(terr, pos);
    }
}

void Game::defaultEditorWindow() {
    ImGui::TextUnformatted("No entity selected");
    if (ImGui::Button("Pathfinder Tool")) {
        mActiveTool = std::make_unique<NavMesh::PathfinderTool>(*this);
    }
    ImGui::SameLine();
    if (ImGui::Button("Unit Spawn Tool")) {
        mActiveTool = std::make_unique<Combat::SpawnTool>(mECS);
    }

    if (ImGui::TreeNodeEx("Scenes", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto pos = mCamera.mPos;
        auto rot = mCamera.spawnRotation();
        ImGui::Text("Rotation: %.2f %.2f %.2f %.2f", rot.x, rot.y, rot.z, rot.w);

        ImGui::InputInt("Seed", &mSceneSeed);
        if (ImGui::Button("Terrain scene")) {
            terrainScene({
                pos, rot * tg::quat::from_axis_angle(tg::dir3(0, 1, 0), 135_deg)
            }, false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Demo Object scene")) {
            mECS.demoSys->addScene(mSceneSeed, pos, rot);
        }
        if (ImGui::Button("Humanoid debug")) {
            auto ent = mECS.newEntity();
            mECS.editables.emplace(ent, &*mECS.combatSys);
            mECS.staticRigids.emplace(ent, ECS::Rigid(pos - 2.f * tg::mat3(rot)[2], rot));
            mECS.humanoids.emplace(ent, Combat::Humanoid());
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Obstacle Test")) {
        auto res = mECS.obstacleSys->closest(mCamera.mPos);
        if (res) {
            ImGui::Text("Closest obstacle is %" PRIu32 " (%.3f)", res->first, res->second);
        }
        auto ray = tg::ray3(mCamera.mPos, mouseWorldDirection());
        res = mECS.obstacleSys->rayCast(ray);
        if (res) {
            auto point = ray[res->second];
            ImGui::Text("Under cursor: obstacle %" PRIu32, res->first);
            ImGui::Text("at %.3f %.3f %.3f (dist %.3f)", point.x, point.y, point.z, res->second);
        } else {
            ImGui::TextUnformatted("No obstacles under cursor");
        }
        ImGui::TreePop();
    }
}

void Game::onGui() {
    if (!mDevMode) {return;}
    if (ImGui::Begin("View"))
    {
        if (ImGui::TreeNodeEx("Controls", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextUnformatted("WASD  - Move");
            ImGui::TextUnformatted("Q/E   - Move up and down");
            ImGui::TextUnformatted("Shift - Speed up");
            ImGui::TextUnformatted("Ctrl  - Slow down");
            ImGui::TextUnformatted("RMB   - Mouselook");
            ImGui::TextUnformatted("LMB   - Select objects");
            ImGui::TextUnformatted("Esc   - Deselect tool/object");
            ImGui::NewLine();
            auto const mousePos = input().getMousePosition();
            ImGui::Text("Cursor pos: %.2f %.2f", mousePos.x, mousePos.y);
            auto const mouseDelta = input().getMouseDeltaF();
            ImGui::Text("Cursor delta: %.2f %.2f", mouseDelta.x, mouseDelta.y);
            ImGui::Checkbox("Capture mouse during mouselook", &mCaptureMouseOnMouselook);
            ImGui::TreePop();
        }
        bool paused = mPaused;
        if (ImGui::Checkbox("Pause", &paused)) {pause(paused);}
        mCamera.updateUI();
        ImGui::Checkbox("Show Wireframe", &mShowWireframe);
        mPostProcess.updateUI();
        mSsao.onGui();
        ImGui::ColorEdit3("Background", &mRenderTargets.mBackgroundColor.r);
    }
    ImGui::End();

    if (ImGui::Begin("Editor")) {
        auto ent = mECS.selectedEntity;
        if (mActiveTool) {
            mActiveTool->updateUI();
        } else if (ent != ECS::INVALID) {
            auto iter = mECS.editables.find(ent);
            if (iter != mECS.editables.end()) {
                iter->second->editorUI(ent);
            } else {
                ImGui::Text("No editor for entity %" PRIu32, ent);
            }
        } else {
            defaultEditorWindow();
        }
    }
    ImGui::End();

    if (ImGui::Begin("Stats")) {
        float buf[STAT_FRAMES];
        for (size_t i = 0;i < STAT_FRAMES; ++i) {
            buf[i] = mFrameStats[(i + mCurFrameStat + 1) % STAT_FRAMES].time * 1000.f;
        }
        gamedev::ImGuiValueGraph(buf, STAT_FRAMES - 1, "Frame time", "", "ms", 300, 65);
    }
    ImGui::End();

    if(ImGui::Begin("Environment")) {
        mLightingSettings.onGui();
        mWindSettings.onGui();
    }
    ImGui::End();
}

// Called when window is resized
void Game::onResize(int w, int h)
{
    mCamera.mAspect = (float)w / h;
    mRenderTargets.resize(w, h);
    mECS.waterSys->resize(tg::isize2(w, h));
    mSsao.resize(w, h);
    mPostProcess.resize(w, h);
    mECS.spriteRendererSys->resize(w, h);
}

// called once per frame
void Game::updateCamera(float elapsedSeconds)
{
    if (mCamera.mControlMode == Camera::ScriptControlled) {return;}

    float speedMultiplier = 15.f;

    // shift / ctrl: speed up and slow down camera movement
    if (isKeyDown(GLFW_KEY_LEFT_SHIFT))
        speedMultiplier *= 8.f;

    if (isKeyDown(GLFW_KEY_LEFT_CONTROL))
        speedMultiplier *= 0.25f;

    auto const move = tg::vec3(
        (isKeyDown(GLFW_KEY_A) ? 1.f : 0.f) - (isKeyDown(GLFW_KEY_D) ? 1.f : 0.f), // x: left and right (A/D keys)
        (isKeyDown(GLFW_KEY_E) ? 1.f : 0.f) - (isKeyDown(GLFW_KEY_Q) ? 1.f : 0.f), // y: up and down (E/Q keys)
        (isKeyDown(GLFW_KEY_W) ? 1.f : 0.f) - (isKeyDown(GLFW_KEY_S) ? 1.f : 0.f)  // z: forward and back (W/S keys)
    );

    // if RMB down and UI does not capture it: hide mouse and move camera
    tg::vec3 rot;
    bool rightMB = isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
    if (rightMB && !ImGui::GetIO().WantCaptureMouse) {
        // capture mouse
        if (mCaptureMouseOnMouselook) {
            setCursorMode(glow::glfw::CursorMode::Disabled);
        }

        auto mouse_delta = input().getMouseDelta();
        rot = tg::vec3(-mouse_delta.y, -mouse_delta.x, 0.0) * 0.001f;
    } else {
        // uncapture mouse
        if (mCaptureMouseOnMouselook) {
            setCursorMode(glow::glfw::CursorMode::Normal);
        }
    }
    auto join = ECS::Join(mECS.staticRigids, mECS.terrains);
    auto iter = join.begin();
    while (iter != join.end()) {
        auto [rig, terr, id] = *iter;
        auto localPos = ~rig * mCamera.mPos;
        auto size = terr.segmentSize * terr.segmentsAmount;
        if (
            localPos.x < 0.f || localPos.x > size
            || localPos.y < -size || localPos.y > size
            || localPos.z < 0.f || localPos.z > size
        ) {
            ++iter;
            continue;
        }
        break;
    }
    mCamera.update(elapsedSeconds, move * -speedMultiplier, rot);
    if (iter != join.end() && !mDevMode) {
        auto [rig, terr, id] = *iter;
        auto size = terr.segmentSize * terr.segmentsAmount;
        auto mat = tg::mat4x3(rig);
        auto localPos = ~rig * mCamera.mPos;
        auto minDist = size * .05f, minTerrainDist = 1.f;
        if (localPos.x < minDist) {
            mCamera.mPos += (minDist - localPos.x) * mat[0];
        } else if (localPos.x > size - minDist) {
            mCamera.mPos += (size - minDist - localPos.x) * mat[0];
        }
        if (localPos.z < minDist) {
            mCamera.mPos += (minDist - localPos.z) * mat[2];
        } else if (localPos.z > size - minDist) {
            mCamera.mPos += (size - minDist - localPos.z) * mat[2];
        }
        if (localPos.y > size - minDist) {
            mCamera.mPos += (size - minDist - localPos.y) * mat[1];
        } else {
            auto minHeight = std::max(terr.getElevationAtPos(localPos.x, localPos.z), terr.waterLevel) + minTerrainDist;
            if (localPos.y < minHeight) {
                mCamera.mPos += (minHeight - localPos.y) * mat[1];
            }
        }
    }
}

ECS::entity Game::readPickingBuffer(int x, int y)
{
    // note: this is naive, efficient readback in OpenGL should be done with a PBO and n-buffering
    uint32_t readValue = ECS::INVALID;
    {
        auto fb = mRenderTargets.mFramebufferReadback->bind();
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &readValue);
    }
    return readValue;
}
