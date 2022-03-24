// SPDX-License-Identifier: MIT
#version 420

#include "../lighting.glsli"

#if INSTANCED
#include "../worldobject/instanced_wo_frag.glsl"
#else
#include "../worldobject/wo_frag.glsl"
#endif

uniform vec3 uCamPos;

in vec3 vWorldPos;
flat in vec3 vNormal;
flat in vec4 vAlbedo;

out vec4 fColor;

void main() {
    init_wo();
    fColor = vec4(computeLighting(
        vWorldPos, uCamPos, vNormal,
        vAlbedo.rgb, vec3(1, .95, 0)
    ), 1);
}
