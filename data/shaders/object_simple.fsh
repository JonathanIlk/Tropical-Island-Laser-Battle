
uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;

uniform vec3 uLightPos;
uniform vec3 uCamPos;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec2 vTexCoord;

out vec3 fColor;
out uint fPickID;

void main()
{
    // local dirs
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangent);
    vec3 B = normalize(cross(N, T));

    const vec2 uv = vTexCoord;

    // unpack normal map
    vec3 normalMap = texture(uTexNormal, uv).xyz;
    normalMap = (normalMap * 2.0) - 1.0;

    // apply normal map
    N = normalize(mat3(T, B, N) * normalMap);

    // render the normal for debugging
    //fColor = saturate(N); return;

    // read color texture (albedo)
    vec3 albedo = texture(uTexAlbedo, uv).rgb;

    // compute cosine of angle between surface normal and incoming light direction.
    vec3 lightdir = normalize(uLightPos - vWorldPos);
    const float lightcontribution = clamp(dot(N, lightdir), 0.0, 1.0);

    vec3 lightcolor = vec3(1.0, 0.9, 0.75);

    // ambient lighting is always applied, regardless of any light sources
    vec3 ambientlight = 0.1 * lightcolor;
    vec3 resultColor = albedo * ambientlight;
    resultColor += lightcolor * lightcontribution;

    // write outputs
    fColor = max(resultColor, vec3(0.0));
}
