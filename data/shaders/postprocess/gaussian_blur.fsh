#version 420
out vec4 color;

in vec2 vTexCoord;

uniform sampler2DRect uInputTex;
uniform sampler1D uKernelTex;
uniform ivec2 uSampleDirection;

void main() {
    ivec2 pos = ivec2(vTexCoord * textureSize(uInputTex));
    vec3 result = vec3(0.0, 0.0, 0.0);
    for(int i = 0; i < textureSize(uKernelTex, 0); ++i) {
        vec3 val = texelFetch(uInputTex, pos + uSampleDirection * i).rgb;
        val += texelFetch(uInputTex, pos - uSampleDirection * i).rgb;
        result += texelFetch(uKernelTex, i, 0).r * val;
    }
    color = vec4(result, 1.0);
}
