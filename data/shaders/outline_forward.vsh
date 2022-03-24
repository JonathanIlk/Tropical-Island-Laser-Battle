uniform mat4 uView;
uniform mat4 uProj;

// this is an affine transform! we can omit the last row for efficiency
uniform mat4x3 uModel;

in vec3 aPosition;

void main()
{
    vec3 worldPos = uModel * vec4(aPosition, 1.0);
    gl_Position = uProj * uView * vec4(worldPos, 1);
}
