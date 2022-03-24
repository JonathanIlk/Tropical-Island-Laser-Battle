// SPDX-License-Identifier: MIT
#version 430

layout(triangles) in;
in VertexData {
    vec3 vWorldPos;
    vec2 vTexPos;
} vtx[3];

layout(triangle_strip, max_vertices=3) out;
out vec3 vWorldPos;
out vec2 vTexPos;
flat out vec3 vNormal;

void main() {
    gl_PrimitiveID = gl_PrimitiveIDIn;
    vNormal = normalize(cross(vtx[2].vWorldPos - vtx[1].vWorldPos, vtx[0].vWorldPos - vtx[1].vWorldPos));
    for (uint i = 0; i < 3; ++i) {
        gl_Position = gl_in[i].gl_Position;
        vWorldPos = vtx[i].vWorldPos;
        vTexPos = vtx[i].vTexPos;
        EmitVertex();
    }
}
