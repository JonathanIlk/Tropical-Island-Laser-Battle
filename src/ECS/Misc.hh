// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>

#include <ECS.hh>

namespace ECS {
class Editor {
public:
    virtual void editorUI(entity) = 0;
};
struct Rigid {
    tg::pos3 translation = {0, 0, 0};
    tg::quat rotation = {0, 0, 0, 1};

    Rigid(tg::pos3 translation = {0, 0, 0}, tg::quat rotation = {0, 0, 0, 1}) : translation{translation}, rotation{rotation} {}

    tg::mat4x3 transform_mat() const;
    explicit operator tg::mat4x3() const {return transform_mat();}
    Rigid chain(const Rigid &) const;
    Rigid interpolate(const Rigid &, float) const;

    Rigid operator~() const;
};

inline Rigid operator*(const Rigid &a, const Rigid &b) {return a.chain(b);}
tg::vec3 operator*(const Rigid &a, const tg::vec3 &b);
tg::dir3 operator*(const Rigid &a, const tg::dir3 &b);
tg::pos3 operator*(const Rigid &a, const tg::pos3 &b);
}
