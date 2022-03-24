// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/types/color.hh>
namespace ColorUtil
{
inline tg::color3 hsv(float hue, float saturation, float value)
{
    float s = saturation / 100;
    float v = value / 100;
    float C = s * v;
    float X = C * (1 - abs(fmod(hue / 60.0, 2) - 1));
    float m = v - C;
    float r, g, b;
    if (hue >= 0 && hue < 60)
    {
        r = C, g = X, b = 0;
    }
    else if (hue >= 60 && hue < 120)
    {
        r = X, g = C, b = 0;
    }
    else if (hue >= 120 && hue < 180)
    {
        r = 0, g = C, b = X;
    }
    else if (hue >= 180 && hue < 240)
    {
        r = 0, g = X, b = C;
    }
    else if (hue >= 240 && hue < 300)
    {
        r = X, g = 0, b = C;
    }
    else
    {
        r = C, g = 0, b = X;
    }
    float R = (r + m);
    float G = (g + m);
    float B = (b + m);
    return tg::color3(R, G, B);
}

inline tg::color3 hsv(tg::vec3 hsv) { return ColorUtil::hsv(hsv.x, hsv.y, hsv.z); }
}
