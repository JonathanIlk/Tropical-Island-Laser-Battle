// SPDX-License-Identifier: MIT
#version 420

uniform mat4 uViewProj;
uniform sampler2D uTexAlbedo;
uniform mat4x3 uModel;

uniform vec4 uClippingPlane;

const int MAX_BONES = 17;
uniform vec4 uBonesRotations[MAX_BONES];
uniform vec4 uBonesTranslations[MAX_BONES];

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;
in ivec4 aBoneIds;
in vec4 aBoneWeights;

out vec3 vWorldPos;
flat out vec3 vNormal;
flat out vec4 vAlbedo;

mat3 matrixFromQuat(vec4 r) {
    mat3 rotationMatrix;
    rotationMatrix[0][0]  = 1.0 - 2.0 * (r.y*r.y + r.z*r.z);
    rotationMatrix[0][1]  = 2.0 * (r.x * r.y + r.z * r.w);
    rotationMatrix[0][2] = 2 * (r.x * r.z - r.y * r.w);

    rotationMatrix[1][0]  =     2.0 * (r.x * r.y - r.z * r.w);
    rotationMatrix[1][1]  = 1.0 - 2.0 * (r.x * r.x + r.z * r.z);
    rotationMatrix[1][2]  =     2.0 * (r.y * r.z + r.x * r.w);

    rotationMatrix[2][0]  =     2.0 * (r.x * r.z + r.y * r.w);
    rotationMatrix[2][1]  =     2.0 * (r.y * r.z - r.x * r.w);
    rotationMatrix[2][2] = 1.0 - 2.0 * (r.x * r.x + r.y * r.y);
    return rotationMatrix;
}

mat4x3 createBoneMatrix(vec4 rotation, vec3 translation) {
    mat3 rotationMat = matrixFromQuat(rotation);
    mat4x3 boneMat;
    boneMat[0] = rotationMat[0];
    boneMat[1] = rotationMat[1];
    boneMat[2] = rotationMat[2];
    boneMat[3] = translation;
    return boneMat;
}

void main()
{
    vAlbedo = vec4(texture(uTexAlbedo, aTexCoord).rgb, 1);

    mat4x3 boneMat = createBoneMatrix(uBonesRotations[aBoneIds.x], uBonesTranslations[aBoneIds.x].xyz) * aBoneWeights.x;
    boneMat += createBoneMatrix(uBonesRotations[aBoneIds.y], uBonesTranslations[aBoneIds.y].xyz) * aBoneWeights.y;
    boneMat += createBoneMatrix(uBonesRotations[aBoneIds.z], uBonesTranslations[aBoneIds.z].xyz) * aBoneWeights.z;
    boneMat += createBoneMatrix(uBonesRotations[aBoneIds.w], uBonesTranslations[aBoneIds.w].xyz) * aBoneWeights.w;

    vec3 deformedPosition = boneMat * vec4(aPosition, 1.0);
    vec3 deformedNormal = mat3(boneMat) * aNormal;
    vec3 deformedTangent = mat3(boneMat) * aTangent;

    vWorldPos = uModel * vec4(deformedPosition, 1.0);
    gl_ClipDistance[0] = dot(vWorldPos, uClippingPlane.xyz) + uClippingPlane.w;
    vNormal = mat3(uModel) * deformedNormal;


    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
