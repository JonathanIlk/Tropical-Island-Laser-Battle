// SPDX-License-Identifier: MIT
uniform mat4 uViewProj;
uniform mat4x3 uModel;

uniform vec4 uClippingPlane;

in vec3 aPosition;
in float aValue;

out vec3 vWorldPos;
out float vValue;

void main()
{
    vValue = aValue;
    vWorldPos = uModel * vec4(aPosition, 1.0);
    gl_ClipDistance[0] = dot(vWorldPos, uClippingPlane.xyz) + uClippingPlane.w;
    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
