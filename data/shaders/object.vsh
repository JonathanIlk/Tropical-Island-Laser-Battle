uniform mat4 uViewProj;

// this is an affine transform! we can omit the last row for efficiency
uniform mat4x3 uModel;

in vec3 aPosition;
in vec3 aNormal;
in vec3 aTangent;
in vec2 aTexCoord;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec2 vTexCoord;

void main()
{
    // assume uModel has no non-uniform scaling
    vNormal = mat3(uModel) * aNormal;
    vTangent = mat3(uModel) * aTangent;

    vTexCoord = aTexCoord;

    vWorldPos = uModel * vec4(aPosition, 1.0);
    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
