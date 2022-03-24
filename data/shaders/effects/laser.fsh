// SPDX-License-Identifier: MIT
#include "../math.glsli"
uniform uint uPickID;
uniform vec4 uColor, uColor2;

in vec2 vSpritePos;

out vec4 fColor;
out vec4 fBrightness;
out uint fPickID;

void main() {
    float rimFadeOut = mix(1.0, 0.0, pow2(vSpritePos.y));
    vec4 laserColor = mix(vec4(uColor.xyz, rimFadeOut), vec4(uColor2.xyz, rimFadeOut), abs(sin(vSpritePos.x * 4.0)));
    fBrightness = laserColor;
    fColor = saturate(laserColor + 0.2);
    fPickID = uPickID;
}
