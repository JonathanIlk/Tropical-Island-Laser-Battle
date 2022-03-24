#version 420

#include "../util/depth_reconstruct.glsli"

const int MAX_KERNEL_SIZE = 16;

uniform sampler2DRect uDepthTex;
uniform sampler2D uNoiseTex;
uniform sampler1D uSsaoKernelTex;
uniform mat4 uInvProjMat;
uniform mat4 uProjMat;

uniform float uRadius;
uniform float uBias;

in vec2 vTexCoord;

out float ao;

vec3 calcViewSpaceNormal(vec3 vsPosOriginal, vec2 texCoord) {
    vec2 texSize = textureSize(uDepthTex);
    const vec2 offset1 = vec2(1 / texSize.x,0);
    const vec2 offset2 = vec2(0,1 / texSize.y);

    vec3 p0 = vsPosOriginal;
    vec3 p1 = getVsPosFromTexCoord(texCoord + offset1, uDepthTex, uInvProjMat);
    vec3 p2 = getVsPosFromTexCoord(texCoord + offset2, uDepthTex, uInvProjMat);

    vec3 hDeriv = p2 - p0;
    vec3 vDeriv = p1 - p0;

    vec3 normal = normalize(cross(vDeriv, hDeriv));
    return normal;
}

void main() {
    vec3 vsPos = getVsPosFromTexCoord(vTexCoord.xy, uDepthTex, uInvProjMat);
    vec3 vsNormal = calcViewSpaceNormal(vsPos, vTexCoord.xy);

    vec2 ssaoNoiseScale = textureSize(uDepthTex) / 4;
    vec3 randomVec = texture(uNoiseTex, vTexCoord.xy * ssaoNoiseScale).xyz;
    vec3 vsTangent   = normalize(randomVec - vsNormal * dot(randomVec, vsNormal));
    vec3 vsBiTangent = cross(vsNormal, vsTangent);
    mat3 TBN = mat3(vsTangent, vsBiTangent, vsNormal); // Base transformation

    float occlusion = 0.0;
    for (int i = 0; i < MAX_KERNEL_SIZE; i++) {
        // go to random kernel pos inside sample hemisphere
        vec3 randomAddition = TBN * texelFetch(uSsaoKernelTex, i, 0).xyz;

        // compute view-space position of random sample
        vec3 vsSamplePos = vsPos + randomAddition * uRadius;

        // Calculate textureCoord of samplePos to obtain vsCoords.
        vec3 ndcSamplePos = homogenize(uProjMat * vec4(vsSamplePos, 1.0));
        vec2 texSamplePos = ((ndcSamplePos.xy + vec2(1.0)) / 2.0).xy;

        // Find the view-space coord of a possible blocker in front of the samplePos
        vec3 vsBlockerPos = getVsPosFromTexCoord(texSamplePos, uDepthTex, uInvProjMat);

        // weight nearer samples more than samples further away (https://learnopengl.com/Advanced-Lighting/SSAO)
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(vsPos.z - vsSamplePos.z));
        occlusion       += (vsSamplePos.z >= vsBlockerPos.z + uBias * vsBlockerPos.z ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = occlusion / MAX_KERNEL_SIZE;

    ao = occlusion;
}
