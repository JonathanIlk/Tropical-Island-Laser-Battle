// SPDX-License-Identifier: MIT
#pragma once

#include <typed-geometry/tg.hh>

struct AnimationEasing
{
private:
    tg::vec2 mP1;
    tg::vec2 mP2;

public:
    static const AnimationEasing easeInOut;
    static const AnimationEasing easeInOutFast;

    AnimationEasing(tg::vec2 p1, tg::vec2 p2) : mP1{p1}, mP2{p2} {};
    float ease(float t) const;
};
