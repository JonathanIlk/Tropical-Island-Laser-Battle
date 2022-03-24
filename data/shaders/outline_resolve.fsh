
#include "math.glsli"

uniform sampler2DRect uTexOutline;

out vec4 fColor;

#define LINE_WIDTH 2.0

void main()
{
    if (any(texture(uTexOutline, gl_FragCoord.xy).rgb))
        discard;

    float dx = LINE_WIDTH;
    float dy = LINE_WIDTH;

    vec4 sampled_color = vec4(0.0);

    sampled_color = texture(uTexOutline, gl_FragCoord.xy + vec2(-dx, -dy));
    if (any(sampled_color.rgb)) 
    {
        fColor = sampled_color;
        return;
    }

    sampled_color = texture(uTexOutline, gl_FragCoord.xy + vec2(-dx,  dy));
    if (any(sampled_color.rgb)) 
    {
        fColor = sampled_color;
        return;
    }

    sampled_color = texture(uTexOutline, gl_FragCoord.xy + vec2( dx, -dy));
    if (any(sampled_color.rgb)) 
    {
        fColor = sampled_color;
        return;
    }

    sampled_color = texture(uTexOutline, gl_FragCoord.xy + vec2( dx,  dy));
    if (any(sampled_color.rgb)) 
    {
        fColor = sampled_color;
        return;
    }

    discard;
}
