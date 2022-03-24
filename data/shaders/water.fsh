// SPDX-License-Identifier: MIT
#include "lighting.glsli"

uniform uint uPickID;
uniform sampler2DRect uRefractTex;
uniform sampler2DRect uReflectTex;
uniform vec3 uCamPos;
uniform vec2 uViewPortSize;

in vec3 vWorldPos;
flat in vec3 vNormal;
in vec2 vTexPos;

out vec4 fColor;
out uint fPickID;

void main() {
    fPickID = uPickID;

    vec3 refractColor = texture(uRefractTex, vTexPos.xy / 2).rgb;
    vec3 reflectColor = texture(uReflectTex, vTexPos.xy / 2).rgb;

    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 fresnel = fresnelSchlick(vec3(M_F_DIELECTRIC), saturate(dot(vNormal, V)));
    fColor = vec4(lerp(refractColor, reflectColor, fresnel), 1);
}
