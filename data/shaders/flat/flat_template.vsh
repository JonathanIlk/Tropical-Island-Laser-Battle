// SPDX-License-Identifier: MIT
uniform mat4 uViewProj;
uniform sampler2D uTexAlbedo;

#if INSTANCED
#include "../worldobject/instanced_wo_vtx.glsl"
#else
#include "../worldobject/wo_vtx.glsl"
#endif

uniform vec4 uClippingPlane;

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;

out vec3 vWorldPos;
flat out vec3 vNormal;
flat out vec4 vAlbedo;

void main()
{
    init_wo();
    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;

    vAlbedo = vec4(texture(uTexAlbedo, aTexCoord).rgb, 1);

    vWorldPos = uModel * vec4(aPosition, 1.0);
    gl_ClipDistance[0] = dot(vWorldPos, uClippingPlane.xyz) + uClippingPlane.w;

    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
