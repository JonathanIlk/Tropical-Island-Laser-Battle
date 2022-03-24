
out vec2 vTexCoord;

void main()
{
    // creates a fullscreen triangle from just the vertex ID
    vTexCoord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(vTexCoord * 2 - 1, 0, 1);
}
