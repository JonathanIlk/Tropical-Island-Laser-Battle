// SPDX-License-Identifier: MIT
#version 430
#include "lighting.glsli"

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexARM;
uniform vec3 uCamPos;
uniform uint uPickingID;
uniform vec4 uAlbedoBias;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec2 vTexCoord;

out vec4 fColor;
out uint fPickID;

void main()
{
    vec3 N = normalize(vNormal), T = normalize(vTangent);
    vec3 B = normalize(cross(N, T));
    const vec2 uv = vTexCoord;

    vec3 normalMap = texture(uTexNormal, uv).xyz;
    normalMap = (normalMap * 2.0) - 1.0;
    N = normalize(mat3(T, B, N) * normalMap);


    // read color texture and apply bias
    vec3 albedo = texture(uTexAlbedo, uv).rgb;
    albedo = saturate(lerp(albedo, uAlbedoBias.rgb, uAlbedoBias.a));

    // read PBR parameters from ARM texture (_A_mbient Occlusion/_R_oughness/_M_etalness in R/G/B channels)
    vec3 arm = texture(uTexARM, uv).rgb;
    arm.g = clamp(arm.g, 0.025, 1.0); // prevent singularities at 0 roughness

    fColor = vec4(computeLighting(uCamPos, vWorldPos, N, albedo, arm), 1);
    fPickID = uPickingID;
}
