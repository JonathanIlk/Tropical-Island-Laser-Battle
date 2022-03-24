// SPDX-License-Identifier: MIT
uniform mat4 uViewProj;
uniform mat4x3 uModel;
uniform mat4 uView;

in vec3 aPosition;
in vec3 aNormal;

out vec3 vWsCenter;
out vec3 vWorldPos;
flat out vec3 vNormal;
flat out vec4 vAlbedo;

void main()
{
    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;

    vAlbedo = vec4(0.1, 0.8, 0.2, 1);

    vWsCenter = uModel * vec4(0, 0, 0, 1.0);
    vWorldPos = uModel * vec4(aPosition, 1.0);

    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
