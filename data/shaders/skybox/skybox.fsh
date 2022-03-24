// SPDX-License-Identifier: MIT
#version 420

#define ENABLE_POINTLIGHT       0
#include "../lighting.glsli"
#include "../noise2D.glsli"
#include "atmosphere.glsli"

uniform sampler2D uNormalsTex;
uniform sampler2D uNormals2Tex;
uniform uint uPickID;
out uint fPickID;

uniform vec3 uCamPos;
uniform float uTime;
uniform float uWaterLevel;
uniform vec3 uSandColor;

in vec3 vWsCenter;
in vec3 vWorldPos;
flat in vec3 vNormal;
flat in vec4 vAlbedo;

out vec4 fColor;

const float sandNormalStrength = 1;
const float waterNormalStrength = 0.08;

vec3 calcSkyColor(vec3 rayDir) {

    float planetScale = 1;

    vec3 pSun = uLighting.sunDirection.xyz;
    vec3 normalizedRayDir = normalize(rayDir);
    vec3 r0 = vWsCenter + vec3(0,6372e3 * planetScale,0);

    vec3 atmosphereColor = atmosphere(
    normalizedRayDir,           // normalized ray direction
    r0,               // ray origin
    pSun,                        // position of the sun
    length(uLighting.sunRadiance) * 10,                           // intensity of the sun
    6371e3 * planetScale,                         // radius of the planet in meters
    6471e3 * planetScale,                         // radius of the atmosphere in meters
    vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
    21e-6,                          // Mie scattering coefficient
    8e3,                            // Rayleigh scale height
    1.2e3,                          // Mie scale height
    0.758                           // Mie preferred scattering direction
    );

    float dotProd = dot(uLighting.sunDirection.xyz, normalizedRayDir);
    float acosResult = acos(dotProd);

    float sunSize = 0.07;
    float isSun = 1.0 - smoothstep(sunSize * 0.5, sunSize, acosResult);
    atmosphereColor = lerp(atmosphereColor, saturate(uLighting.sunRadiance.xyz * 2), isSun);

    return atmosphereColor;
}

vec3 loadMixedNormals(vec2 pos1, vec2 pos2, float strength, float lerpVal) {
    vec3 N = vec3(0.0, 1.0, 0.0);
    vec3 T = vec3(1.0, 0.0, 0.0);
    vec3 B = vec3(0.0, 0.0, 1.0);
    vec3 normal1 = texture(uNormalsTex, pos1).xyz;
    vec3 normal2 = texture(uNormals2Tex, pos2).xyz;
    normal1 = lerp(normal1, normal2, lerpVal);
    normal1 = (normal1 * 2.0) - 1.0;
    normal1.xy *= strength;
    normal1 = normalize(mat3(T, B, N) * normal1);
    return normal1;
}

vec3 loadSandNormals(vec2 pos1, float strength, float lerpVal) {
    vec3 normal1 = texture(uNormals2Tex, pos1).xyz;
    normal1 = (normal1 * 2.0) - 1.0;
    normal1.xy *= strength;
    return normal1;
}

void main() {
    vec3 waterLevelCenter = vWsCenter + vec3(0.0, uWaterLevel, 0.0);

    fPickID = uPickID;

    vec3 wsLookDirection = normalize(uCamPos - vWorldPos);
    vec3 wsWaterPlaneNormal = vec3(0, 1, 0);

    // Horizon Water
    float denom = dot(wsWaterPlaneNormal, wsLookDirection);
    vec3 color = vec3(0.0, 0.0, 0.0);
    vec3 contribution = vec3(1, 1, 1);
    vec3 skyDir = -wsLookDirection;
    if (denom > 0.0001f) // avoid 0
    {
        float horizonDistance = dot(abs(waterLevelCenter - uCamPos), wsWaterPlaneNormal) / denom;
        if (horizonDistance >= 0.1) {
            vec3 camToHorizon = wsLookDirection * horizonDistance;
            vec3 camToCenter = uCamPos - vWsCenter;
            vec3 wsHorizon = camToCenter - camToHorizon;

            vec3 sandNormal = loadSandNormals((wsHorizon.xz / 73), sandNormalStrength, sin(wsHorizon.x  + wsHorizon.y));
            vec3 waterNormal = loadMixedNormals((wsHorizon.xz / 73), ((wsHorizon.zx + vec2(17, 19)) / 47), waterNormalStrength, sin(uTime * 1.5 + wsHorizon.x + wsHorizon.y));

            vec3 sandPatternColor = computeLighting(vec3(0.0,0.0,0.0), uLighting.sunDirection.xyz, sandNormal, uSandColor * 0.7, vec3(1, .95, 0));
            vec3 farSeaColor = vec3(0.25, 0.88, 0.816) * 0.1;
            vec3 refractColor = lerp(sandPatternColor, farSeaColor, clamp(length(wsHorizon) / 750, 0, 1));
            if (vWorldPos.y < uWaterLevel) {
                // Sand Color
                color = refractColor;
            } else {
                skyDir -= (2 * dot(skyDir, wsWaterPlaneNormal)) * wsWaterPlaneNormal;

                vec3 V = normalize(camToHorizon);
                vec3 fresnel = fresnelSchlick(vec3(M_F_DIELECTRIC), saturate(dot(waterNormal, V)));
                color = refractColor * (1 - fresnel);
                contribution = fresnel;
            }

        }
    }

    color += contribution * calcSkyColor(skyDir);

    fColor = vec4(color, 1);
}
