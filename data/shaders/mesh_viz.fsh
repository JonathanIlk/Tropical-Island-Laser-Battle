// SPDX-License-Identifier: MIT
uniform uint uPickID;
uniform float uLuminance;

in vec3 vWorldPos;
in vec3 vViewPos;
in vec3 vNormal;

out vec4 fColor;
out uint fPickID;

void main() {
    fColor = vec4(normalize(vNormal) * uLuminance + vec3(uLuminance), 1);
    fPickID = uPickID;
}
