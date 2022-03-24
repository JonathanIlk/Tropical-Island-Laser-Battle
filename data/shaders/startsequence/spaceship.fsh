// SPDX-License-Identifier: MIT
#version 420

#include "../lighting.glsli"
#include "../worldobject/wo_frag.glsl"

uniform sampler2D uTexAlbedo;
uniform vec3 uCamPos;

in vec3 vWorldPos;
flat in vec3 vNormal;
in vec2 vTexCoord;

out vec4 fColor;

void main() {
    init_wo();
    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    fColor = vec4(computeLighting(
        vWorldPos, uCamPos, vNormal,
        albedo, vec3(1, 0.25, 0.2)
    ), 1);
}
