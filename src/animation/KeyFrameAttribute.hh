// SPDX-License-Identifier: MIT
#pragma once

#include <typed-geometry/tg-lean.hh>

namespace KeyFrameAttribute
{

    template <typename T>
    struct Attribute
    {
    public:
        T value;
        Attribute() = default;
        explicit Attribute(T val) : value{val} {};
        T interpolateTo(Attribute<T> nextFrame, float t);
    };

};
