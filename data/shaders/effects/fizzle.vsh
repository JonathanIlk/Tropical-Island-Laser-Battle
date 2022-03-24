// SPDX-License-Identifier: MIT
#include "../math.glsli"
uniform vec3 uCamPos;
uniform mat4 uViewProj;
uniform vec3 uUp;
uniform float uFadeBase, uFadeSlope, uSize;
uniform float uTime;

// instance data
in vec3 aCenter;
in vec3 aDrift;
in float aTimeOffset;

// vertex data
in vec2 aSpritePos;

out vec2 vSpritePos;
flat out float vAlpha;

void main() {
    vec3 center = aCenter + uTime * aDrift;
    vec3 normal = normalize(uCamPos - center);
    vec3 right = normalize(cross(uUp, normal));
    vec3 up = cross(normal, right);


    vAlpha = saturate((uTime + aTimeOffset) * uFadeSlope + uFadeBase);
    vSpritePos = aSpritePos;

    vec2 pos = aSpritePos * uSize;
    vec3 worldPos = center + pos.x * right + pos.y * up;
    gl_Position = uViewProj * vec4(worldPos, 1);
}
