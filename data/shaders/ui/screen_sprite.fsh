// SPDX-License-Identifier: MIT
#version 420

uniform sampler2D uImage;
uniform float uAlpha;

in vec2 vTexCoord;

out vec4 fColor;

void main()
{
    vec4 imageColor = texture(uImage, vTexCoord);
    fColor = vec4(imageColor.rgb, imageColor.a * uAlpha);
}
