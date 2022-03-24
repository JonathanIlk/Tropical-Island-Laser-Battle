// SPDX-License-Identifier: MIT
#version 420

#include "./lighting.glsli"

uniform uint uPickID;
uniform vec3 uCamPos;

in vec3 vWorldPos;
flat in vec3 vNormal;
flat in vec4 vAlbedo;
in float vAlpha;

out vec4 fColor;
out uint fPickID;


void main() {
    fPickID = uPickID;
    fColor = vec4(computeLighting(
    vWorldPos, uCamPos, vNormal,
    vAlbedo.rgb, vec3(1, .95, 0)
    ), vAlpha);
}
