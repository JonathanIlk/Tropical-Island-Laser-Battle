// SPDX-License-Identifier: MIT
uniform mat3x2 uTransform;

in vec2 aPosition;

out vec2 vTexCoord;

void main() {
    vTexCoord = aPosition;
    gl_Position = vec4(uTransform * vec3(aPosition.x, -aPosition.y, 1.0), 0., 1.);
}
