// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <random>
#include <typed-geometry/tg.hh>

namespace Util {

inline tg::quat angv2quat(tg::vec3 angv) {
    float angle_sqr = tg::length_sqr(angv);
    if (angle_sqr == 0.0) {return tg::quat::identity;}
    return tg::quat::from_axis_angle(
        tg::normalize(angv),
        tg::angle_t<float>::from_radians(sqrt(angle_sqr))
    );
}

inline float srgb2linear(uint8_t value) {
    return value <= 10
        ? value * (25.f / 323 / 255)
        : tg::pow(((200.f / 211 / 255) * value + (11.f / 211)), 2.4f);
}

inline tg::vec4 unpackSRGBA(uint32_t value) {
    return tg::vec4(
        srgb2linear((value >> 24) & 0xFF),
        srgb2linear((value >> 16) & 0xFF),
        srgb2linear((value >> 8) & 0xFF),
        float((value >> 0) & 0xFF) / 255.f
    );
}

inline tg::mat4x3 transformMat(tg::pos3 translation, tg::quat rotation = {0, 0, 0, 1}, tg::size3 scaling = {1, 1, 1}) {
    // note: this is significantly faster than three naive matrix muls!
    auto const rot_mat = tg::mat3(rotation);
    tg::mat4x3 M;
    M[0] = rot_mat[0] * scaling[0];
    M[1] = rot_mat[1] * scaling[1];
    M[2] = rot_mat[2] * scaling[2];
    M[3] = tg::vec3(translation);
    return M;
}

inline tg::mat4 transformMat4(tg::pos3 translation, tg::quat rotation = {0, 0, 0, 1}, tg::size3 scaling = {1, 1, 1}) {
    auto mat = tg::mat4(transformMat(translation, rotation, scaling));
    mat[3][3] = 1.0;
    return mat;
}

inline tg::pos3 randomPositionOnTriangle(std::mt19937& rng, tg::pos3 p1, tg::pos3 p2, tg::pos3 p3) {
    std::uniform_real_distribution<> uni(0.0, 1.0);
    auto a = uni(rng);
    auto b = uni(rng);

    return (1-sqrt(a))*p1 + (sqrt(a)*(1 - b))*p2 + (b*sqrt(a))*p3;
}

inline tg::vec3 triangleNormal(tg::pos3 p1, tg::pos3 p2, tg::pos3 p3) {
    return tg::normalize(tg::cross(p2-p1, p3-p1));
}

inline tg::quat fromToRotation(tg::vec3 fromAxis, tg::vec3 toAxis) {
    auto cross = tg::cross(fromAxis, toAxis);
    if (cross == tg::vec3::zero) {
        return tg::dot(fromAxis, toAxis) >= 0
            ? tg::quat::identity  // same axis
            : tg::quat(1, 0, 0, 0);  // opposite axis
    }
    auto axis = tg::normalize(cross);
    tg::angle angle = tg::angle_between(fromAxis, toAxis);
    return tg::quat::from_axis_angle(axis, angle);
}

inline tg::quat forwardUpOrientation(tg::vec3 fwd, tg::vec3 up) {
    auto right = tg::cross(fwd, up);
    if (right == tg::vec3::zero) {
        // the vectors are colinear: just take any orientation conforming to fwd
        return fromToRotation(tg::vec3(0, 0, -1), fwd);
    }
    right = tg::normalize(right);
    fwd = tg::normalize(fwd);
    up = tg::normalize(tg::cross(right, fwd));
    return tg::quat::from_rotation_matrix(tg::mat3(right, up, -fwd));
}
inline tg::quat rightForwardOrientation(tg::vec3 right, tg::vec3 fwd) {
    auto up = tg::cross(right, fwd);
    if (up == tg::vec3::zero) {
        // the vectors are colinear: just take any orientation conforming to fwd
        return fromToRotation(tg::vec3(1, 0, 0), right);
    }
    right = tg::normalize(right);
    up = tg::normalize(up);
    fwd = tg::normalize(tg::cross(up, right));
    return tg::quat::from_rotation_matrix(tg::mat3(right, up, -fwd));
}
inline tg::quat upForwardOrientation(tg::vec3 up, tg::vec3 fwd) {
    auto right = tg::cross(fwd, up);
    if (right == tg::vec3::zero) {
        // the vectors are colinear: just take any orientation conforming to up
        return fromToRotation(tg::vec3(0, 1, 0), up);
    }
    right = tg::normalize(right);
    up = tg::normalize(up);
    fwd = tg::normalize(tg::cross(up, right));
    return tg::quat::from_rotation_matrix(tg::mat3(right, up, -fwd));
}

inline tg::quat lookAtOrientation(const tg::pos3& from, const tg::pos3& to, const tg::vec3& up) {
    return Util::forwardUpOrientation(tg::normalize(to - from), tg::vec3::unit_y);
}

}
