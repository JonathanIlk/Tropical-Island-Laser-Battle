// SPDX-License-Identifier: MIT
uniform mat4 uViewProj;
uniform mat4x3 uModel;

in vec3 aPosition;
in vec3 aNormal;

out vec3 vWorldPos;
out vec3 vViewPos;
out vec3 vNormal;

void main()
{
    vWorldPos = uModel * vec4(aPosition, 1.0);
    gl_Position = uViewProj * vec4(vWorldPos, 1);
    vNormal = aNormal;
}
