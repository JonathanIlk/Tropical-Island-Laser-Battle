// SPDX-License-Identifier: MIT
#pragma once

#include <map>
#include <string>
#include <typed-geometry/types/pos.hh>
#include <typed-geometry/types/quat.hh>
#include "KeyFrameAttribute.hh"
#include <vector>

struct AbstractKeyFrame
{
    float mTime;
    inline explicit AbstractKeyFrame(float time) : mTime(time) {}
    AbstractKeyFrame() = default;
};

struct RigidKeyFrame final : public AbstractKeyFrame
{
    KeyFrameAttribute::Attribute<tg::pos3> mPosition;
    KeyFrameAttribute::Attribute<tg::quat> mRotation;
    RigidKeyFrame() = default;
    inline RigidKeyFrame(float time, tg::pos3 position, tg::quat rotation) : AbstractKeyFrame(time), mPosition(position), mRotation{rotation} {}
};

struct IntroCameraKeyFrame final : public AbstractKeyFrame
{
    KeyFrameAttribute::Attribute<tg::pos3> mPosition;
    KeyFrameAttribute::Attribute<tg::quat> mRotation;
    KeyFrameAttribute::Attribute<float> mLookAtShip;
    inline IntroCameraKeyFrame(float time, tg::pos3 position, tg::quat rotation, float lookAtShip)
      : AbstractKeyFrame(time), mPosition(position), mRotation{rotation}, mLookAtShip(lookAtShip) {}
};

struct FloatKeyFrame final : public AbstractKeyFrame {
    KeyFrameAttribute::Attribute<float> mValue;
    inline FloatKeyFrame(float time, float value) : AbstractKeyFrame(time), mValue(value) {}
};

struct BonesKeyFrame final : public AbstractKeyFrame
{
    std::vector<RigidKeyFrame> mValues;
    inline BonesKeyFrame(float time, std::vector<RigidKeyFrame> values) : AbstractKeyFrame(time), mValues(values) {}
};
