#include "./util/depth_reconstruct.glsli"

uniform sampler2DRect uTexColor;
uniform sampler2DRect uTexBloom;
uniform sampler2DRect uTexDepth;

uniform bool uEnableTonemap;
uniform bool uEnableGamma;

uniform float uWsFocusDistance;
uniform float uStartDoFDistance;
uniform float uFinalDoFDistance;
uniform sampler2DRect uBlurredTexColor;
uniform mat4 uInvProjMat;

uniform vec2 uVignetteBorder;
uniform vec4 uVignetteColor;

in vec2 vTexCoord;

out vec3 fColor;

//
// Some Tonemappers to play with
//

//
// Uncharted 2 Tonemapper

vec3 uc2_transform(in vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 tonemap_uc2(in vec3 color)
{
    float W = 11.2;

    color *= 16;  // Hardcoded Exposure Adjustment

    float exposure_bias = 2.0f;
    vec3 curr = uc2_transform(exposure_bias*color);

    vec3 white_scale = vec3(1,1,1)/uc2_transform(vec3(W,W,W));
    vec3 ccolor = curr*white_scale;

    return ccolor;
}

//
// Filmic tonemapper

vec3 tonemap_filmic(in vec3 color)
{
    color = max(vec3(0,0,0), color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f)+ 0.06f);

    return color;
}

//
// Metal Gear Solid V tonemapper

vec3 tonemap_mgsv(in vec3 color)
{
    float A = 0.6;
    float B = 0.45333;
    vec3 mapped = min(vec3(1,1,1), A + B - ( (B * B) / (color - A + B) ));
    vec3 condition = vec3(color.r > A, color.g > A, color.b > A);

    return mapped * condition + color * (1 - condition);
}


// accurate linear to sRGB conversion (often simplified to pow(x, 1/2.24))
vec3 applySRGBCurve(vec3 x)
{
    // accurate linear to sRGB conversion (often simplified to pow(x, 1/2.24))
    return mix(
        vec3(1.055) * pow(x, vec3(1.0/2.4)) - vec3(0.055), 
        x * vec3(12.92), 
        lessThan(x, vec3(0.0031308))
    );
}

vec3 applyDepthOfField() {
    vec3 color = texture(uTexColor, gl_FragCoord.xy).rgb;
    vec3 blurredColor = texture(uBlurredTexColor, gl_FragCoord.xy).rgb;


    vec3 vsPos = getVsPosFromTexCoord(vTexCoord, uTexDepth, uInvProjMat);

    float blur = smoothstep( uStartDoFDistance, uFinalDoFDistance, abs(uWsFocusDistance + vsPos.z));

    return mix(color, blurredColor, blur);
}

void main()
{
    // sample scene HDR color and depth
    vec3 color = applyDepthOfField();
    vec3 bloomColor = texture(uTexBloom, gl_FragCoord.xy).rgb;
    color += bloomColor;
    float depth = texture(uTexDepth, gl_FragCoord.xy).x;


    if (uEnableTonemap) {
        color = tonemap_filmic(color);
    }
    vec2 quadrantCoords = abs(gl_FragCoord.xy * (2. / textureSize(uTexColor)) - 1);
    vec2 closestInside = min(quadrantCoords, uVignetteBorder);
    color = mix(color, uVignetteColor.rgb, uVignetteColor.a * clamp(length((quadrantCoords - closestInside) / (1 - uVignetteBorder)), 0., 1.));
    if (uEnableGamma) {
        color = applySRGBCurve(color);
    }

    fColor = color;
}
