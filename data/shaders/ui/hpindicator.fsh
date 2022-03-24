// SPDX-License-Identifier: MIT
#include "../lighting.glsli"
uniform vec3 uCamPos;
uniform uint uPickID;
uniform float uValue;
uniform vec3 uLeftColor;
uniform vec3 uRightColor;
uniform vec3 uAlbedo;

in vec3 vWorldPos;
in float vValue;

out vec4 fColor;
out uint fPickID;

void main() {
    fPickID = uPickID;
    vec3 normal = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
    float grad = length(vec2(dFdx(vValue), dFdy(vValue)));
    float param = clamp((vValue - uValue) / grad + .5, 0., 1.);
    fColor = vec4(computeLighting(
        vWorldPos, uCamPos, normal,
        uAlbedo, vec3(1, .95, 0)
    ) + mix(uLeftColor, uRightColor, param), 1);
}
