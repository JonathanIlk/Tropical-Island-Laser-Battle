// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <queue>
#include <vector>
#include "Animation.hh"
#include "AnimationEasing.hh"
#include "KeyFrame.hh"

template<typename Frame_T>
struct QueuedAnimation {
    std::shared_ptr<Animation<Frame_T>> animation;
    float animStartTime = 0;
};

class AbstractAnimator {
protected:
    int currentKeyFrameIdx = 0;
    virtual void updateCurrentKeyFrame() = 0;
    bool isLinear = true;
    AnimationEasing easing = AnimationEasing::easeInOut;
    float interpolationValueBetween(float frame1Time, float frame2Time, float time) const;
public:
    virtual ~AbstractAnimator() {}
    float mAnimationTime = 0.f;
    bool loop = false;
    bool finished = false;
    void update(float deltaSeconds);
    void reset();
    void setEasing(AnimationEasing animEasing);
};

template<typename Frame_T>
class Animator : public AbstractAnimator
{
    std::queue<QueuedAnimation<Frame_T>> mAnimationQueue;
public:
    explicit Animator(std::shared_ptr<Animation<Frame_T>> animation);

    std::shared_ptr<Animation<Frame_T>> mAnimation;
    virtual Frame_T interpolateBetween(Frame_T& frame1, Frame_T& frame2, float time);
    virtual Frame_T currentState();
    virtual Frame_T& getNextKeyFrame();
    void updateCurrentKeyFrame();
    void enqueAnimation(std::shared_ptr<Animation<Frame_T>> anim, float animStartTime = 0.0f);
    void setNewAnimation(std::shared_ptr<Animation<Frame_T>> anim) {
        this->mAnimation = anim;
        reset();
    }
};

class RiggedAnimator final : public Animator<BonesKeyFrame> {
    std::map<std::string, glow::assimp::BoneData>& mBonesMap;
    std::string& mRootBoneName;
public:
    explicit RiggedAnimator(std::shared_ptr<RiggedAnimation> animation, std::map<std::string, glow::assimp::BoneData>& bonesMap,
                            std::string& rootBoneName);
    BonesKeyFrame interpolateBetween(BonesKeyFrame& frame1, BonesKeyFrame& frame2, float time);
    void calculateBoneTransform(
        std::vector<RigidKeyFrame>& lerpVals, glow::assimp::BoneData& bone, tg::mat4 parentTransform, BonesKeyFrame& frame1, BonesKeyFrame& frame2, float t);
    tg::mat4 getLocalBoneTransform(glow::assimp::BoneData& bone, BonesKeyFrame& frame1, BonesKeyFrame& frame2, float t);
};
