// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>

class Camera {
public:
    tg::pos3 mPos = tg::pos3();
    tg::quat mOrient = tg::quat::identity;
    float mAspect = 1.0, mFocal = 1.0;
    enum ControlMode : int {Free, Upright, AbsoluteVertical, ScriptControlled};
    int mControlMode = Upright;

    void update(float dt, tg::vec3 movement, tg::vec3 rotation);
    void updateUI();
    tg::mat4x3 viewMatrix() const;
    tg::mat4 projectionMatrix() const;
    tg::dir3 ndc2dir(tg::vec2) const;
    tg::quat spawnRotation() const;
};
