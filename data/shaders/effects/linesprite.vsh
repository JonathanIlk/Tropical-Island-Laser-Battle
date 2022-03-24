// SPDX-License-Identifier: MIT
#include "../math.glsli"
uniform vec3 uCamPos;
uniform mat4 uViewProj;

// instance data
in float aWidth;
in vec3 aStart, aEnd;

// vertex data
in vec2 aSpritePos;  // x ∈ [0, 1] along line (main axis), y ∈ [-1, 1] along cross axis

out vec2 vSpritePos;
out vec3 vWorldPos;
flat out vec3 vNormal;

void main() {
    vec3 mainAxis = aEnd - aStart;
    float mainLensq = length2(mainAxis);
    vec3 lineToCam = uCamPos - aStart;
    lineToCam -= (dot(lineToCam, mainAxis) / mainLensq) * mainAxis;

    vec3 crossAxis = cross(mainAxis, lineToCam);
    float crossLensq = length2(crossAxis);
    float widthFactor = crossLensq > 0 ? aWidth / sqrt(crossLensq) : 0;

    vSpritePos = aSpritePos;
    vNormal = normalize(lineToCam);
    vWorldPos = aStart + aSpritePos.x * mainAxis;
    vWorldPos += (aSpritePos.y * widthFactor) * crossAxis;
    gl_Position = uViewProj * vec4(vWorldPos, 1);
}
