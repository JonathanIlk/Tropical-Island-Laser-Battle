#include "Animator.hh"
#include <MathUtil.hh>
#include <algorithm>
#include <glow/common/log.hh>
#include <utility>

float AbstractAnimator::interpolationValueBetween(float frame1Time, float frame2Time, float time) const
{
    if (frame1Time == frame2Time)
    {
        return 1;
    }

    float interpolationVal = (time - frame1Time) / (frame2Time - frame1Time);

    if (!isLinear)
    {
        interpolationVal = easing.ease(interpolationVal);
    }
    return interpolationVal;
}

void AbstractAnimator::update(float deltaSeconds) {
    mAnimationTime += deltaSeconds;
    updateCurrentKeyFrame();
}

void AbstractAnimator::reset() {
    currentKeyFrameIdx = 0;
    finished = false;
    mAnimationTime = 0.0f;
}

void AbstractAnimator::setEasing(AnimationEasing animEasing) {
    this->isLinear = false;
    this->easing = animEasing;
}

template<typename Frame_T>
void Animator<Frame_T>::updateCurrentKeyFrame()
{
    if (currentKeyFrameIdx == mAnimation->lastKeyFrameIdx)
    {
        if (!mAnimationQueue.empty())
        {
            auto queuedAnim = mAnimationQueue.front();
            mAnimationQueue.pop();
            mAnimation = queuedAnim.animation;
            reset();
            mAnimationTime = queuedAnim.animStartTime;
            return;
        }
        if (loop)
        {
            reset();
        }
        else
        {
            finished = true;
        }
        // Stay at last frame
        return;
    }
    while (mAnimationTime > getNextKeyFrame().mTime && currentKeyFrameIdx != mAnimation->lastKeyFrameIdx)
    {
        currentKeyFrameIdx++;
    }
}

template<typename Frame_T>
Animator<Frame_T>::Animator(std::shared_ptr<Animation<Frame_T>> animation) {
    this->mAnimation = std::move(animation);
}

template<typename Frame_T>
Frame_T& Animator<Frame_T>::getNextKeyFrame() {
    return mAnimation->keyFrames[std::min(currentKeyFrameIdx + 1, mAnimation->lastKeyFrameIdx)];
}

template<typename Frame_T>
Frame_T Animator<Frame_T>::currentState() {
    auto& currentKeyFrame = mAnimation->keyFrames[currentKeyFrameIdx];
    auto& nextKeyFrame = getNextKeyFrame();
    return interpolateBetween(currentKeyFrame, nextKeyFrame, mAnimationTime);
}

template <typename Frame_T>
void Animator<Frame_T>::enqueAnimation(std::shared_ptr<Animation<Frame_T>> anim, float startAtTime)
{
    mAnimationQueue.push({
        anim,
        startAtTime
    });
}

#pragma region RigidKeyFrame
template<>
RigidKeyFrame Animator<RigidKeyFrame>::interpolateBetween(RigidKeyFrame& frame1, RigidKeyFrame& frame2, float time) {
    float t = interpolationValueBetween(frame1.mTime, frame2.mTime, time);
    return RigidKeyFrame(time,
                         frame1.mPosition.interpolateTo(frame2.mPosition, t),
                         frame1.mRotation.interpolateTo(frame2.mRotation, t));
}
template RigidKeyFrame Animator<RigidKeyFrame>::currentState();
template RigidKeyFrame& Animator<RigidKeyFrame>::getNextKeyFrame();
template Animator<RigidKeyFrame>::Animator(std::shared_ptr<Animation<RigidKeyFrame>> animation);
template void Animator<RigidKeyFrame>::enqueAnimation(std::shared_ptr<Animation<RigidKeyFrame>> anim, float startAtTime);
#pragma endregion RigidKeyFrame

#pragma region FloatKeyFrame
template<>
FloatKeyFrame Animator<FloatKeyFrame>::interpolateBetween(FloatKeyFrame& frame1, FloatKeyFrame& frame2, float time) {
    float t = interpolationValueBetween(frame1.mTime, frame2.mTime, time);
    return FloatKeyFrame(time, frame1.mValue.interpolateTo(frame2.mValue, t));
}
template FloatKeyFrame Animator<FloatKeyFrame>::currentState();
template FloatKeyFrame& Animator<FloatKeyFrame>::getNextKeyFrame();
template Animator<FloatKeyFrame>::Animator(std::shared_ptr<Animation<FloatKeyFrame>> animation);
template void Animator<FloatKeyFrame>::enqueAnimation(std::shared_ptr<Animation<FloatKeyFrame>> anim, float startAtTime);
#pragma endregion FloatKeyFrame

#pragma region IntroCameraKeyFrame
template<>
IntroCameraKeyFrame Animator<IntroCameraKeyFrame>::interpolateBetween(IntroCameraKeyFrame& frame1, IntroCameraKeyFrame& frame2, float time) {
    float t = interpolationValueBetween(frame1.mTime, frame2.mTime, time);
    return IntroCameraKeyFrame(time,
                         frame1.mPosition.interpolateTo(frame2.mPosition, t),
                         frame1.mRotation.interpolateTo(frame2.mRotation, t),
                         frame1.mLookAtShip.interpolateTo(frame2.mLookAtShip, t));
}
template IntroCameraKeyFrame Animator<IntroCameraKeyFrame>::currentState();
template IntroCameraKeyFrame& Animator<IntroCameraKeyFrame>::getNextKeyFrame();
template Animator<IntroCameraKeyFrame>::Animator(std::shared_ptr<Animation<IntroCameraKeyFrame>> animation);
template void Animator<IntroCameraKeyFrame>::enqueAnimation(std::shared_ptr<Animation<IntroCameraKeyFrame>> anim, float startAtTime);
#pragma endregion IntroCameraKeyFrame

#pragma region RiggedAnimator
class StringException : public std::exception {
    const char *message;
public:
    StringException(const char *message) : message{message} {}
    const char *what() const noexcept override {return message;}
};

template<> BonesKeyFrame Animator<BonesKeyFrame>::interpolateBetween(BonesKeyFrame& frame1, BonesKeyFrame& frame2, float time) {
    throw StringException("For BonesKeyFrames use RiggedAnimator.");
};

RiggedAnimator::RiggedAnimator(std::shared_ptr<RiggedAnimation> animation, std::map<std::string, glow::assimp::BoneData>& bonesMap, std::string& rootBoneName)
  : Animator<BonesKeyFrame>(animation), mBonesMap(bonesMap), mRootBoneName(rootBoneName) {}

BonesKeyFrame RiggedAnimator::interpolateBetween(BonesKeyFrame& frame1, BonesKeyFrame& frame2, float time) {
    float t = interpolationValueBetween(frame1.mTime, frame2.mTime, time);
    std::vector<RigidKeyFrame> lerpVals(mBonesMap.size());

    auto& rootBone = mBonesMap[mRootBoneName];

    calculateBoneTransform(lerpVals, rootBone, tg::mat4::identity, frame1, frame2, t);

    return BonesKeyFrame(time, lerpVals);
}
void RiggedAnimator::calculateBoneTransform(std::vector<RigidKeyFrame>& lerpVals, glow::assimp::BoneData& bone,
                                            tg::mat4 parentTransform, BonesKeyFrame& frame1, BonesKeyFrame& frame2, float t)
{
    auto localTransform = getLocalBoneTransform(bone, frame1, frame2, t);
    tg::mat4 globalTransform = parentTransform * localTransform;
    tg::mat4 finalTransform = globalTransform * bone.offsetMat;

    lerpVals[bone.id].mRotation.value = tg::quat::from_rotation_matrix(finalTransform);
    lerpVals[bone.id].mPosition.value = tg::pos3(finalTransform[3]);

    for(const auto& childName : bone.childBones) {
        calculateBoneTransform(lerpVals, mBonesMap[childName], globalTransform, frame1, frame2, t);
    }
}

tg::mat4 RiggedAnimator::getLocalBoneTransform(glow::assimp::BoneData& bone, BonesKeyFrame& frame1, BonesKeyFrame& frame2, float t) {
    auto& boneId = bone.id;
    RigidKeyFrame& frame1Bone = frame1.mValues[boneId];
    RigidKeyFrame& frame2Bone = frame2.mValues[boneId];

    return Util::transformMat4(frame1Bone.mPosition.interpolateTo(frame2Bone.mPosition, t), frame1Bone.mRotation.interpolateTo(frame2Bone.mRotation, t));
}
template void Animator<BonesKeyFrame>::enqueAnimation(std::shared_ptr<Animation<BonesKeyFrame>> anim, float startAtTime);
#pragma endregion BonesKeyFrame
