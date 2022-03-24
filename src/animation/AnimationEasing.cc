#include "AnimationEasing.hh"
float AnimationEasing::ease(float t) const {
    return (powf((1.0f- t), 3.0f) * tg::vec2(0,0)
            + 3 * powf(1- t, 2)*t*mP1
            + 3 * (1-t) * powf(t, 2) * mP2
              + powf(t, 3) * tg::vec2(1, 1)).y;
}

const AnimationEasing AnimationEasing::easeInOut(tg::vec2(0.3, 0), tg::vec2(0.7, 1));
const AnimationEasing AnimationEasing::easeInOutFast(tg::vec2(0.1, 0), tg::vec2(0.9, 1));
