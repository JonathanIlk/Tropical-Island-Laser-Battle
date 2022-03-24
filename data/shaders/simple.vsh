// SPDX-License-Identifier: MIT
#version 400

uniform mat4 uViewProj;
uniform mat4x3 uModel;

in vec3 aPosition;

out VertexData {
    vec3 vWorldPos;
};

void main() {
    vWorldPos = uModel * vec4(aPosition, 1);
    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
