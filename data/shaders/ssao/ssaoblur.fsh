#version 420
out float ao;

in vec2 vTexCoord;

uniform sampler2DRect ssaoInput;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput));
    float result = 0.0;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y));
            result += texture(ssaoInput, gl_FragCoord.xy + offset).r;
        }
    }
    ao = result / (4.0 * 4.0);
}
