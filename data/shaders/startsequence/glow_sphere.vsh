// SPDX-License-Identifier: MIT
#version 430

#include "../noise2D.glsli"

uniform mat4 uViewProj;

// this is an affine transform! we can omit the last row for efficiency
uniform mat4x3 uModel;
uniform float uTime;

in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;

out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    vNormal = normalize(mat3(uModel) * aNormal);

    vec3 deformation = (snoise(aPosition.xy + uTime) * vNormal * 0.1);

    vWorldPos = uModel * vec4(aPosition, 1.0) + deformation;
    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
