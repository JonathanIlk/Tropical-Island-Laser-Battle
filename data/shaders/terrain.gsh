// SPDX-License-Identifier: MIT
#version 430

uniform vec4 uClippingPlane;

layout(triangles) in;
in VertexData {
    in vec3 vModelPos;
    in vec3 vWorldPos;
    in vec3 vColor;
    in float vAlpha;
} vtx[3];

layout(triangle_strip, max_vertices=3) out;
out vec3 vWorldPos;
flat out vec3 vNormal;
flat out vec4 vAlbedo;
out float vAlpha;

void main() {
    gl_PrimitiveID = gl_PrimitiveIDIn;
    vNormal = normalize(cross(vtx[2].vWorldPos - vtx[1].vWorldPos, vtx[0].vWorldPos - vtx[1].vWorldPos));
    vAlbedo = vec4((vtx[0].vColor + vtx[1].vColor + vtx[2].vColor) / 3, 1);
    for (uint i = 0; i < 3; ++i) {
        gl_Position = gl_in[i].gl_Position;
        vWorldPos = vtx[i].vWorldPos;
        gl_ClipDistance[0] = dot(vWorldPos, uClippingPlane.xyz) + uClippingPlane.w;
        vAlpha = vtx[i].vAlpha;
        EmitVertex();
    }
}
