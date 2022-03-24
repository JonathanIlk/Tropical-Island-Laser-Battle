// SPDX-License-Identifier: MIT
#include "Camera.hh"
#include <complex>

#include <typed-geometry/tg.hh>
#include <glow/common/log.hh>
#include <imgui/imgui.h>
#include <MathUtil.hh>

void Camera::updateUI() {
    float pos[3] = {mPos.x, mPos.y, mPos.z};
    if (ImGui::InputFloat3("Cam pos", pos)) {
        mPos = {pos[0], pos[1], pos[2]};
    }
    auto rot = tg::mat3(mOrient);
    auto fwd = -rot[2];
    auto len = mOrient.x*mOrient.x + mOrient.y*mOrient.y + mOrient.z*mOrient.z + mOrient.w*mOrient.w;
    ImGui::Text("Quat: %.2f %.2f %.2f %.2f", mOrient[0], mOrient[1], mOrient[2], mOrient[3]);
    ImGui::Text("forward: %.2f %.2f %.2f", fwd[0], fwd[1], fwd[2]);
    auto up = rot[1];
    ImGui::Text("up: %.2f %.2f %.2f", up[0], up[1], up[2]);
    auto right = rot[0];
    ImGui::Text("right: %.2f %.2f %.2f", right[0], right[1], right[2]);
    ImGui::SliderFloat("Focal distance", &mFocal, 0.5, 20);
    ImGui::RadioButton("Free", &mControlMode, Free);
    ImGui::SameLine();
    ImGui::RadioButton("Upright", &mControlMode, Upright);
    ImGui::SameLine();
    ImGui::RadioButton("Absolute", &mControlMode, AbsoluteVertical);
}

tg::mat4x3 Camera::viewMatrix() const {
    auto rot = (tg::mat3)tg::conjugate(mOrient);
    auto trans = rot * (tg::pos3::zero - mPos);
    return tg::mat4x3(rot[0], rot[1], rot[2], trans);
}

tg::mat4 Camera::projectionMatrix() const {
    //return tg::perspective_opengl(60_deg, mAspect, 0.01f, 500.f);
    auto res = tg::mat4::zero;
    res[0][0] = mFocal;
    res[1][1] = mFocal * mAspect;
    // 1132 = Edge to edge for 800 width terrain.
    float far_plane = 1132, near_plane = 0.01f;
    res[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
    res[3][2] = -2 * near_plane * far_plane / (far_plane - near_plane);
    res[2][3] = -1;
    return res;
}

tg::dir3 Camera::ndc2dir(tg::vec2 ndc) const {
    tg::vec3 viewCoords(ndc.x, ndc.y / mAspect, -mFocal);
    return tg::normalize(tg::vec3(mOrient * tg::quat(viewCoords) * tg::conjugate(mOrient)));
}

void Camera::update(float dt, tg::vec3 linv, tg::vec3 angv) {
    mOrient = tg::normalize(mOrient * Util::angv2quat(angv));

    auto rot = tg::mat3(mOrient);
    auto right = rot[0], up = rot[1], fwd = -rot[2];
    if (mControlMode != Free && fabs(fwd.y) < 0.99) {
        right = tg::vec3(tg::normalize(tg::cross(fwd, tg::vec3(0, 1, 0))));
        up = tg::vec3(tg::normalize(tg::cross(right, fwd)));
        mOrient = tg::quat::from_rotation_matrix(tg::mat3(right, up, -fwd));
    }
    linv *= dt;
    if (mControlMode != AbsoluteVertical) {
        mPos += rot * linv;
    } else {
        mPos.y += linv.y;  // height is controlled directly
        // the right vector is kept horizontal, forward direction is perpendicular to that
        mPos += linv.x * right + linv.z * tg::cross(right, tg::vec3(0, 1, 0));
    }
}

tg::quat Camera::spawnRotation() const {
    if (mControlMode == Free) {return mOrient;}
    auto rot = tg::mat3(mOrient);
    auto fwd = rot[2];
    if (fwd[0] == 0 && fwd[2] == 0) {fwd = -rot[1];}
    return tg::quat::from_axis_angle(tg::dir3(0, 1, 0), tg::angle::from_radians(std::log(std::complex(fwd.z, fwd.x)).imag()));
}
