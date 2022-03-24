// SPDX-License-Identifier: MIT
#include "StartSequence.hh"

#include <ECS/Join.hh>
#include <Game.hh>
#include <MathUtil.hh>
#include <Mesh3D.hh>
#include <animation/AnimatorManager.hh>
#include <combat/SpawnTool.hh>
#include <glow-extras/geometry/Quad.hh>
#include <glow/objects.hh>
#include <terrain/Terrain.hh>
#include <ui/SpriteRenderer.hh>


using namespace StartSequence;

System::System(Game& game) : mGame{game}
{
    mFlatShader = glow::Program::createFromFiles({"../data/shaders/startsequence/spaceship.fsh", "../data/shaders/startsequence/spaceship.vsh"});
    mGlowShader = glow::Program::createFromFiles({"../data/shaders/startsequence/glow_sphere.fsh", "../data/shaders/startsequence/glow_sphere.vsh"});
    mTexAlbedo = game.mSharedResources.colorPaletteTex;

    mTexLogo = game.mSharedResources.logo;
    mTexTimeAndPlace = glow::Texture2D::createFromFile("../data/textures/intro_time_place.png", glow::ColorSpace::Linear);

    Mesh3D dropshipMesh;
    dropshipMesh.loadFromFile("../data/meshes/dropship.obj", true, false);
    mDropShipVao = dropshipMesh.createVertexArray();

    Mesh3D backThrusters;
    backThrusters.loadFromFile("../data/meshes/dropship_backthrusters.obj", true, false);
    mBackThrustersVao = backThrusters.createVertexArray();

    Mesh3D bottomThrusters;
    bottomThrusters.loadFromFile("../data/meshes/dropship_bottomthrusters.obj", true, false);
    mBottomThrustersVao = bottomThrusters.createVertexArray();
}

void System::startSequence(Terrain::Instance &terrain, const ECS::Rigid &terrainRigid) {
    Instance dropShipInstance;
    terrainMat = terrainRigid.transform_mat();
    spawnPosition = tg::pos3(280.0, 0.0, 280.0);
    float spawnElevation = terrain.getElevationAtPos(280, 280);
    spawnPosition.y = spawnElevation;
    tg::pos3 startPos = tg::pos3(terrainMat * tg::vec4(15.0, spawnPosition.y + 50, 15.0, 1.0));
    tg::pos3 middlePos = tg::pos3(terrainMat * tg::vec4(0.8f * spawnPosition.x, spawnPosition.y + 15, 0.8f * spawnPosition.z, 1.0));
    tg::pos3 endPos = tg::pos3(terrainMat * tg::vec4(spawnPosition.x, spawnPosition.y + 10, spawnPosition.z, 1.0));
    const tg::quat& startFlightDirection = Util::lookAtOrientation(startPos, middlePos, tg::vec3::unit_y);
    const tg::quat& landingFlightDirection = Util::lookAtOrientation(middlePos, endPos + tg::pos3(0, 20, 0), tg::vec3::unit_y);
    RigidKeyFrame startKeyFrame(0.0f, startPos, startFlightDirection);
    RigidKeyFrame middleKeyFrame(0.6f * sequenceRunTime, middlePos, startFlightDirection);
    RigidKeyFrame endKeyFrame(sequenceRunTime, endPos, landingFlightDirection);
    dropShipAnimation = std::make_shared<Animation<RigidKeyFrame>>();
    dropShipAnimation->insertKeyFrame(startKeyFrame);
    dropShipAnimation->insertKeyFrame(middleKeyFrame);
    dropShipAnimation->insertKeyFrame(endKeyFrame);
    dropShipAnimator = std::make_shared<Animator<RigidKeyFrame>>(Animator<RigidKeyFrame>(dropShipAnimation));
    dropShipInstance.animator = dropShipAnimator;
    dropShipInstance.vao = mDropShipVao;
    dropShipInstance.type = Type::DropShip;
    auto dropShipEnt = mGame.mECS.newEntity();

    bottomThrusterAnimation = std::make_shared<Animation<FloatKeyFrame>>();
    bottomThrusterAnimation->insertKeyFrame(FloatKeyFrame(0.0f, 0.0f));
    bottomThrusterAnimation->insertKeyFrame(FloatKeyFrame(0.6f * sequenceRunTime, 0.0f));
    bottomThrusterAnimation->insertKeyFrame(FloatKeyFrame(0.7f * sequenceRunTime, 1.0f));
    bottomThrusterAnimator = std::make_shared<Animator<FloatKeyFrame>>(Animator<FloatKeyFrame>(bottomThrusterAnimation));
    bottomThrusterAnimator->setEasing(AnimationEasing::easeInOut);

    mGame.mECS.staticRigids.emplace(dropShipEnt, ECS::Rigid(
        startKeyFrame.mPosition.value, startKeyFrame.mRotation.value
    ));
    mGame.mECS.startSequenceObjects.emplace(dropShipEnt, dropShipInstance);

    mGame.mCamera.mControlMode = Camera::ScriptControlled;

    cameraAnimation = std::make_shared<Animation<IntroCameraKeyFrame>>();

    cameraAnimation->insertKeyFrames(std::vector(
        {
            IntroCameraKeyFrame(0.0f, tg::pos3(0.0, 30.0, 0.0), tg::quat::identity, 0.0f),
            IntroCameraKeyFrame(0.1f * sequenceRunTime, tg::pos3(0.0, 30.0, 0.0), tg::quat::identity, 0.0f),
            IntroCameraKeyFrame(0.25f * sequenceRunTime, tg::pos3(-5.0, 5.0, -30.0), tg::quat::identity, 1.0f),
            IntroCameraKeyFrame(0.45f * sequenceRunTime, tg::pos3(-30.0, -5.0, 15.0), tg::quat::identity, 1.0f),
            IntroCameraKeyFrame(0.65f * sequenceRunTime, tg::pos3(30.0, 5.0, 30.0), tg::quat::identity, 1.0f),
            IntroCameraKeyFrame(0.85f * sequenceRunTime, tg::pos3(25.0, 20.0, 15.0), tg::quat::identity, 1.0f),
            IntroCameraKeyFrame(1.0f * sequenceRunTime, tg::pos3(5.0, 5.0, -5.0), tg::quat::identity, 1.0f),
        }));
    cameraAnimator = std::make_shared<Animator<IntroCameraKeyFrame>>(Animator<IntroCameraKeyFrame>(cameraAnimation));
    cameraAnimator->setEasing(AnimationEasing::easeInOutFast);

    prepareSprites();
    spawnStartParrots();

    AnimatorManager::start(cameraAnimator);
    AnimatorManager::start(dropShipAnimator);
    AnimatorManager::start(bottomThrusterAnimator);
    isSequenceRunning = true;
}

void System::spawnStartParrots() {
    struct ParrotInfo {
        tg::vec4 offset;
        float animStartTime;
    };
    for(auto parrotInfo : std::initializer_list<ParrotInfo> {
        {tg::vec4(3.0, 30.8, 5.0, 1.0f), 0.17f}, {tg::vec4(4.0, 31.8, 5.25, 1.0f), 0.08f},
        {tg::vec4(5.0, 30.5, 5.5, 1.0f), 0.12f}}) {
        auto parrotEnt = mGame.mECS.newEntity();
        parrotEnts.emplace(parrotEnt, parrotInfo.offset);
        ECS::Rigid rigid = {tg::pos3::zero, tg::quat::identity};
        mGame.mECS.riggedRigids.emplace(parrotEnt, rigid);
        auto parrotInstance = RiggedMesh::Instance(&mGame.mSharedResources.mParrotMesh, mGame.mSharedResources.ANIM_STARTSEQUENCE);
        parrotInstance.animator->loop = false;
        AnimatorManager::start(parrotInstance.animator);
        parrotInstance.animator->mAnimationTime = parrotInfo.animStartTime;
        mGame.mECS.riggedMeshes.emplace(parrotEnt, parrotInstance);
    }
}

void System::prepareSprites()
{
    timeAndPlaceSprite = mGame.mECS.spriteRendererSys->addSprite(
        tg::pos2((float)mGame.getWindowWidth() * 0.05f, (float)mGame.getWindowHeight() * 0.95f),
        tg::size2(1024, 256) * 0.5, mTexTimeAndPlace,
        1.0f,
        tg::vec2(0.0f, 1.0f));
    timeAndPlaceAnimation = std::make_shared<Animation<FloatKeyFrame>>();
    timeAndPlaceAnimation->insertKeyFrames(std::vector(
        {
            FloatKeyFrame(0.0f, 0.f),
            FloatKeyFrame(0.10f * sequenceRunTime, 1.f),
            FloatKeyFrame(0.25f * sequenceRunTime, 1.f),
            FloatKeyFrame(0.35f * sequenceRunTime, 0.f),
        }));

    timeAndPlaceAnimator = std::make_shared<Animator<FloatKeyFrame>>(Animator<FloatKeyFrame>(timeAndPlaceAnimation));
    timeAndPlaceAnimator->setEasing(AnimationEasing::easeInOut);
    AnimatorManager::start(timeAndPlaceAnimator);

    logoSprite = mGame.mECS.spriteRendererSys->addSprite(
        tg::pos2((float)mGame.getWindowWidth() * 0.5f, (float)mGame.getWindowHeight() * 0.05f),
        tg::size2(16, 9) * 35, mTexLogo,
        0.0f,
        tg::vec2(0.5f, 0.0f));
    logoAnimation = std::make_shared<Animation<FloatKeyFrame>>();
    logoAnimation->insertKeyFrames(std::vector(
        {
            FloatKeyFrame(0.0f, 0.f),
            FloatKeyFrame(0.31f * sequenceRunTime, 0.f),
            FloatKeyFrame(0.51f * sequenceRunTime, 1.f),
            FloatKeyFrame(0.65f * sequenceRunTime, 1.f),
            FloatKeyFrame(0.8f * sequenceRunTime, 0.f),
        }));

    logoAnimator = std::make_shared<Animator<FloatKeyFrame>>(Animator<FloatKeyFrame>(logoAnimation));
    logoAnimator->setEasing(AnimationEasing::easeInOut);
    AnimatorManager::start(logoAnimator);


    fadeInOutAnimation = std::make_shared<Animation<FloatKeyFrame>>();
    fadeInOutAnimation->insertKeyFrames(std::vector({
                                                        FloatKeyFrame(0.0f, 0.0f),
                                                        FloatKeyFrame(sequenceRunTime - 6.0f, 0.0f),
                                                        FloatKeyFrame(sequenceRunTime, 1.0f),
                                                        FloatKeyFrame(sequenceRunTime + 1.0f, 1.0f),
                                                        FloatKeyFrame(sequenceRunTime + 4.0f, 0.0f),
                                                    }));
    auto fadeInOutAnimator = std::make_shared<Animator<FloatKeyFrame>>(Animator<FloatKeyFrame>(fadeInOutAnimation));
    fadeInOutAnimator->setEasing(AnimationEasing::easeInOut);
    mGame.mECS.spriteRendererSys->startFadeAnimation(fadeInOutAnimator);
}

void System::applyAnimations(tg::pos3 sunPosition)
{
    if (!isSequenceRunning)
    {
        return;
    }
    if (dropShipAnimator && dropShipAnimator->finished)
    {
        stopSequence();
        return;
    }
    auto& spriteRenderer = mGame.mECS.spriteRendererSys;
    spriteRenderer->setSpriteAlpha(timeAndPlaceSprite, timeAndPlaceAnimator->currentState().mValue.value);
    spriteRenderer->setSpriteAlpha(logoSprite, logoAnimator->currentState().mValue.value);
    for (auto &&tup : ECS::Join(mGame.mECS.staticRigids, mGame.mECS.startSequenceObjects)) {
        auto &[wo, instance, id] = tup;

        if (instance.animator)
        {
            auto currentState = instance.animator->currentState();
            wo.translation = currentState.mPosition.value;
            wo.rotation = currentState.mRotation.value;
            if (instance.type == DropShip)
            {
                // Move camera to follow dropShip;
                Camera& cam = mGame.mCamera;
                const auto& keyFrame = cameraAnimator->currentState();
                cam.mPos = tg::pos3(wo.transform_mat() * tg::vec4(keyFrame.mPosition.value, 1.0f));
                const auto& lookAtShip = Util::lookAtOrientation(cam.mPos, wo.translation, tg::vec3::unit_y);
                const auto& lookAtSun = Util::lookAtOrientation(cam.mPos, sunPosition, tg::vec3::unit_y);
                cam.mOrient = tg::slerp(lookAtSun, lookAtShip, keyFrame.mLookAtShip.value);
                mGame.mPostProcess.mFocusDistance = tg::lerp(5.0f, tg::distance(cam.mPos, wo.translation), keyFrame.mLookAtShip.value);

                for (const auto& [parrotEnt, parrotOffset] : parrotEnts)
                {
                    if (mGame.mECS.riggedRigids.find(parrotEnt) != mGame.mECS.riggedRigids.end())
                    {
                        ECS::Rigid& parrotRigid = mGame.mECS.riggedRigids[parrotEnt];
                        parrotRigid.translation = tg::pos3(wo.transform_mat() * parrotOffset);
                        parrotRigid.rotation = tg::quat::from_rotation_matrix(tg::mat3(wo.transform_mat()));
                        if (mGame.mECS.riggedMeshes[parrotEnt].animator->finished)
                        {
                            mGame.mECS.deleteEntity(parrotEnt);
                        }
                    }
                }
            }
        }
    }

}
void System::stopSequence()
{
    spawnPlayerUnits();

    mGame.mCamera.mControlMode = Camera::ControlMode::AbsoluteVertical;
    isSequenceRunning = false;
    mGame.mECS.spriteRendererSys->removeSprite(timeAndPlaceSprite);
    mGame.mECS.spriteRendererSys->removeSprite(logoSprite);
    AnimatorManager::stop(dropShipAnimator);
    AnimatorManager::stop(cameraAnimator);
    AnimatorManager::stop(logoAnimator);
    AnimatorManager::stop(timeAndPlaceAnimator);
    mGame.mECS.startSequenceObjects.clear();
    parrotEnts.clear();
}

void System::renderMain(MainRenderPass& pass) {
    if (!isSequenceRunning)
    {
        return;
    }

    for (auto &&tup : ECS::Join(pass.snap->rigids, mGame.mECS.startSequenceObjects)) {
        auto &[wo, instance, id] = tup;
        mFlatShader->setUniformBuffer("uLighting", pass.lightingUniforms);
        auto shader = mFlatShader->use();
        pass.applyCommons(shader);

        shader["uModel"] = wo.transform_mat();
        shader["uPickID"] = id;
        shader["uTexAlbedo"] = mTexAlbedo;

        instance.vao->bind().draw();
    }
}

void System::renderTransparent(MainRenderPass& pass) {
    if (!isSequenceRunning)
    {
        return;
    }

    for (auto &&tup : ECS::Join(pass.snap->rigids, mGame.mECS.startSequenceObjects)) {
        auto &[wo, instance, id] = tup;
        auto glowShader = mGlowShader->use();
        pass.applyCommons(glowShader);
        pass.applyTime(glowShader);
        glowShader["uModel"] = wo.transform_mat();

        glowShader["uAlpha"] = 1.0f;
        mBackThrustersVao->bind().draw();

        glowShader["uAlpha"] = bottomThrusterAnimator->currentState().mValue.value;
        mBottomThrustersVao->bind().draw();
    }
}
void System::spawnPlayerUnits() {
    auto spawnTool = Combat::SpawnTool(mGame.mECS);
    int spawnedUnits = 0;
    float spawnRadius = 2.5;
    auto placeOnCircle = 0.0f;
    while (spawnedUnits < 4)
    {
        auto theta = placeOnCircle * 2.0f * 3.1415926f;
        tg::vec3 randomCircleCoordinate(spawnRadius * cos(theta), 0, spawnRadius * sin(theta));

        const auto rayOriginLocalSpace = spawnPosition + tg::pos3(0, 50, 0) + randomCircleCoordinate;
        const auto rayOriginWorldSpace = tg::pos3(terrainMat * tg::vec4(rayOriginLocalSpace, 1.0));
        const tg::ray3 spawnRay = tg::ray3(rayOriginWorldSpace, tg::dir3::neg_y);
        auto newUnit = spawnTool.spawnUnit(spawnRay);
        if (newUnit.has_value())
        {
            spawnedUnits++;
        }
        placeOnCircle += 0.2f;
        if (placeOnCircle > 1.0f)
        {
            placeOnCircle = 0.0f;
            spawnRadius += 1.5f;
        }
    }
}
