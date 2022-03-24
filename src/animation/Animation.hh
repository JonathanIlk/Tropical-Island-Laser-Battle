// SPDX-License-Identifier: MIT
#pragma once
#include <vector>

#include <glow-extras/assimp/MeshData.hh>

#include "KeyFrame.hh"

class AbstractAnimation {
public:
    int lastKeyFrameIdx = -1;
};

template<typename Frame_T>
class Animation : public AbstractAnimation
{
public:
    std::vector<Frame_T> keyFrames;
    void insertKeyFrame(Frame_T frame) {
        this->keyFrames.emplace_back(frame);
        this->lastKeyFrameIdx++;
    }

    void insertKeyFrames(const std::vector<Frame_T>& frames) {
        for(auto frame : frames) {
            insertKeyFrame(frame);
        }
    }
};

class RiggedAnimation : public Animation<BonesKeyFrame> {
public:

    RiggedAnimation static fromLoadedData(glow::assimp::AnimationData& data) {
        RiggedAnimation animation;
        std::size_t currentFrame = 0;
        auto keyFramesAmount = data.boneAnims[0].positionFrames.size();
        while (currentFrame < keyFramesAmount) {
            std::vector<RigidKeyFrame> boneFrames;
            for (std::size_t i = 0; i < data.boneAnims.size(); i++) {
                auto boneAnim = data.boneAnims[i];
                RigidKeyFrame frameForBone(
                    boneAnim.positionFrames[currentFrame].time,
                    boneAnim.positionFrames[currentFrame].value,
                    boneAnim.rotationFrames[currentFrame].value
                );
                boneFrames.emplace_back(frameForBone);
            }
            animation.insertKeyFrame(BonesKeyFrame(data.boneAnims[1].positionFrames[currentFrame].time, boneFrames));
            currentFrame++;
        }

        return animation;
    }
};
