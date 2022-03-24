// SPDX-License-Identifier: MIT
uniform mat4 uViewProj;

#include "../worldobject/wo_vtx.glsl"

uniform vec4 uClippingPlane;

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;

out vec3 vWorldPos;
out vec2 vTexCoord;
flat out vec3 vNormal;

void main()
{
    init_wo();
    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;

    vTexCoord = aTexCoord;

    vWorldPos = uModel * vec4(aPosition, 1.0);
    gl_ClipDistance[0] = dot(vWorldPos, uClippingPlane.xyz) + uClippingPlane.w;

    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
