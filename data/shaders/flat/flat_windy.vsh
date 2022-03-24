// SPDX-License-Identifier: MIT
uniform mat4 uViewProj;
uniform sampler2D uTexAlbedo;

#include "../worldobject/instanced_wo_vtx.glsl"

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;

out vec3 vWorldPos;
flat out vec3 vNormal;
flat out vec4 vAlbedo;

uniform vec4 uClippingPlane;

uniform float uTime;
uniform vec4 uWindSettings; // xy = direction (x,z worldSpace), z = wind frequency, w = strength
uniform float uObjectHeight;
uniform float uWindAffectFrom; // The height from which on the wind affects the object.
uniform sampler2D uWindTex;

vec3 applyWind(vec3 wsPos, vec3 osPos) {
    float affectedFactor = smoothstep(uWindAffectFrom, uObjectHeight, osPos.y);
    vec2 wind = texture(uWindTex, wsPos.xz / 1000 + uTime * -uWindSettings.xy * uWindSettings.z).rg;

    // Calculate the xz movement from the wind
    vec3 windPos = wsPos + vec3(uWindSettings. x * wind.x, 0, uWindSettings.y * wind.y) * uWindSettings.w * affectedFactor;

    // Project the windmovement onto a sphere to get a vertical movement as well.
    vec3 sphereCenter = uModel[3];
    float sphereRadius = length(wsPos - sphereCenter);
    vec3 centerToWindPos = windPos - sphereCenter;
    float len = length(centerToWindPos);
    vec3 projectedVector = (sphereRadius/len) * centerToWindPos;

    return sphereCenter + projectedVector;
}

void main()
{
    init_wo();
    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;

    vAlbedo = vec4(texture(uTexAlbedo, aTexCoord).rgb, 1);

    vWorldPos = applyWind(uModel * vec4(aPosition, 1.0), aPosition);
    gl_ClipDistance[0] = dot(vWorldPos, uClippingPlane.xyz) + uClippingPlane.w;

    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
