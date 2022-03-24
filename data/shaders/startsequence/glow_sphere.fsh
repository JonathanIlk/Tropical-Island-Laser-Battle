// SPDX-License-Identifier: MIT
#version 430

#include "../math.glsli"
#include "../noise2D.glsli"

uniform vec3 uCamPos;
uniform float uTime;
uniform float uAlpha;

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 fColor;
out vec4 fBrightness;
out uint fPickID;

void main()
{
    vec3 uGlowInnerCol = vec3(0.0/255.0, 80.0/255.0, 227.0/255.0);
    vec3 uGlowOuterCol = vec3(232.0/255.0, 0.0/255.0, 238.0/255.0);

    vec3 viewDir = normalize(uCamPos - vWorldPos);

    float fresnel = 1 - saturate(pow((1.0 - saturate(dot(vNormal, viewDir))), 0.08));
    float pulsationFactor = 1 + abs(sin(uTime * 2.5));

    vec3 resultColor = lerp(uGlowOuterCol, uGlowInnerCol, saturate(fresnel * 3.5 * pulsationFactor));

    fColor = vec4(resultColor * 0.0, fresnel * uAlpha);
    fBrightness = vec4(resultColor, saturate(fresnel * 10) * uAlpha);
}

