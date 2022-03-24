// SPDX-License-Identifier: MIT
#version 420

#include "lighting.glsli"

uniform uint uPickID;
uniform vec3 uCamPos;
uniform vec3 uAlbedo;
uniform vec3 uARM;
uniform vec3 uEmission;

in vec3 vWorldPos;
flat in vec3 vNormal;

out vec4 fColor;
out uint fPickID;

void main() {
    fPickID = uPickID;
    fColor = vec4(computeLighting(
        vWorldPos, uCamPos, vNormal,
        uAlbedo, uARM
    ) + uEmission, 1);
}
