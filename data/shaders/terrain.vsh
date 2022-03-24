// SPDX-License-Identifier: MIT
#version 430

uniform uint uRows;
uniform float uSegmentSize;
uniform mat4x3 uModel;
uniform mat4 uViewProj;
uniform float uMinAlpha;
uniform float uTerrainRadius;


in float aHeight;
in vec3 color;

out VertexData {
    vec3 vModelPos;
    vec3 vWorldPos;
    vec3 vColor;
    float vAlpha;
};

void main() {
    uint x = gl_VertexID /  uRows, z = gl_VertexID % uRows;
    vec2 horizontal = vec2(float(x), float(z)) * uSegmentSize;
    vColor = color;
    vModelPos = vec3(horizontal.x, aHeight, horizontal.y);

    float minDistanceFromBorder = min(uTerrainRadius - abs(vModelPos.x - uTerrainRadius), uTerrainRadius - abs(vModelPos.z - uTerrainRadius));
    // Fade out terrain at borders to merge with skybox if uMinAlpha is 0.
    vAlpha = max(uMinAlpha, clamp(minDistanceFromBorder / (uTerrainRadius * 0.35), 0.0, 1.0));
    vWorldPos = uModel * vec4(vModelPos, 1.0);
    gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
