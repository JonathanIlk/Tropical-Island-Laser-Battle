// SPDX-License-Identifier: MIT
#include "noise2D.glsli"

uniform mat4 uViewProj;
uniform mat4x3 uModel;
uniform vec2 uViewPortSize;
uniform uint uRows;
uniform float uSegmentSize, uTime, uWaterLevel, uWaveHeight;
uniform float uTerrainRadius;

out VertexData {
    vec3 vWorldPos;
    vec2 vTexPos;
};

void main()
{
    uint x = gl_VertexID / uRows, z = gl_VertexID % uRows;
    vec2 horizontal = vec2(float(x), float(z)) * uSegmentSize;

    vec3 modelPos = vec3(horizontal.x, uWaterLevel, horizontal.y);

    vec4 glPosNoWaves = uViewProj * vec4(uModel * vec4(modelPos, 1), 1);
    vec3 ndc = glPosNoWaves.xyz / glPosNoWaves.w; //perspective divide/normalize
    vec2 viewportCoord = ndc.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
    vTexPos = viewportCoord * uViewPortSize;

    // Don't apply waves at the border of the terrain to avoid skybox flickering
    float minDistanceFromBorder = min(uTerrainRadius - abs(modelPos.x - uTerrainRadius), uTerrainRadius - abs(modelPos.z - uTerrainRadius));
    float applyWave = step(5.0f, minDistanceFromBorder);

    float waveOffset = snoise(horizontal + uTime) * applyWave;
    modelPos += vec3(0, waveOffset * uWaveHeight, 0);
    vWorldPos = uModel * vec4(modelPos, 1);

    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
