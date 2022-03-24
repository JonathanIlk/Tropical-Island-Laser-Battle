// SPDX-License-Identifier: MIT
#include "../math.glsli"
uniform uint uPickID;
uniform vec4 uColor, uColor2;

out vec4 fColor;
out vec4 fBrightness;
out uint fPickID;

in vec2 vSpritePos;
flat in float vAlpha;

void main() {
    fColor = mix(uColor, uColor2, abs(sin(vSpritePos.x * 4.0)));
    fColor.a *= vAlpha * saturate(1 - length(vSpritePos));
    fBrightness = fColor;
    fPickID = uPickID;
}
