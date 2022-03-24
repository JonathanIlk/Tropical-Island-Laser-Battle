// SPDX-License-Identifier: MIT
#pragma once

#include <Camera.hh>
#include <ECS/Misc.hh>
#include <animation/Animation.hh>
#include <animation/Animator.hh>
#include <glow/fwd.hh>
#include <rendering/MainRenderPass.hh>

namespace StartSequence {

enum Type : int {DropShip, Fluff};

struct Instance {
    glow::SharedVertexArray vao;
    std::shared_ptr<Animator<RigidKeyFrame>> animator;
    Type type;
};

class System final {
    Game& mGame;

    bool isSequenceRunning = false;
    float sequenceRunTime = 45.0f;

    glow::SharedProgram mFlatShader;
    glow::SharedProgram mGlowShader;
    glow::SharedTexture2D mTexAlbedo;
    glow::SharedTexture2D mTexLogo;
    glow::SharedTexture2D mTexTimeAndPlace;
    glow::SharedVertexArray mDropShipVao;
    glow::SharedVertexArray mBackThrustersVao;
    glow::SharedVertexArray mBottomThrustersVao;

    ECS::entity logoSprite;
    ECS::entity timeAndPlaceSprite;

    std::shared_ptr<Animation<RigidKeyFrame>> dropShipAnimation;
    std::shared_ptr<Animator<RigidKeyFrame>> dropShipAnimator;
    std::shared_ptr<Animation<IntroCameraKeyFrame>> cameraAnimation;
    std::shared_ptr<Animator<IntroCameraKeyFrame>> cameraAnimator;
    std::shared_ptr<Animation<FloatKeyFrame>> logoAnimation;
    std::shared_ptr<Animator<FloatKeyFrame>> logoAnimator;
    std::shared_ptr<Animation<FloatKeyFrame>> timeAndPlaceAnimation;
    std::shared_ptr<Animator<FloatKeyFrame>> timeAndPlaceAnimator;
    std::shared_ptr<Animation<FloatKeyFrame>> fadeInOutAnimation;
    std::shared_ptr<Animation<FloatKeyFrame>> bottomThrusterAnimation;
    std::shared_ptr<Animator<FloatKeyFrame>> bottomThrusterAnimator;

    std::map<ECS::entity, tg::vec4> parrotEnts;
public:
    tg::pos3 spawnPosition;
    tg::mat4x3 terrainMat;
    System(Game&);
    void startSequence(Terrain::Instance&, const ECS::Rigid &terrainRigid);

    void applyAnimations(tg::pos3 sunPosition);
    void renderMain(MainRenderPass& pass);
    void renderTransparent(MainRenderPass& pass);

    void stopSequence();
    void prepareSprites();
    void spawnStartParrots();
    void spawnPlayerUnits();
};

}


