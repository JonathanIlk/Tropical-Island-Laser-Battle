// SPDX-License-Identifier: MIT

#include "KeyFrameAttribute.hh"
#include "../../extern/typed-geometry/src/typed-geometry/feature/basic.hh"
#include "../../extern/typed-geometry/src/typed-geometry/feature/quat.hh"

using namespace KeyFrameAttribute;

template<> tg::quat Attribute<tg::quat>::interpolateTo(Attribute<tg::quat> nextFrame, float t) {
    return tg::slerp(value, nextFrame.value, t);
}

template<typename T>
T Attribute<T>::interpolateTo(Attribute<T> nextFrame, float t)
{
    return tg::lerp(value, nextFrame.value, t);
}

template float Attribute<float>::interpolateTo(Attribute<float> nextFrame, float t);
template tg::pos3 Attribute<tg::pos3>::interpolateTo(Attribute<tg::pos3> nextFrame, float t);
