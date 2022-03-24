// SPDX-License-Identifier: MIT
#include "TerrainMaterial.hh"
#include <random>

#include <ColorUtil.h>
#include <MathUtil.hh>

const std::array<TerrainMaterial, TerrainMaterial::NumMaterials> TerrainMaterial::table {{
    {Rock, tg::vec3(27, 35, 52)},
    {Grass, tg::vec3(110, 76, 57)},
    {Sand, tg::vec3(52, 65, 57)}
}};

tg::color3 TerrainMaterial::randomTint(uint32_t seed) const {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> uni(-100, 100);
    float randomTintFactor = (float)(uni(rng) / 100.f);
    float maxTintChange = 12;
    float tintChange = randomTintFactor * maxTintChange;
    return ColorUtil::hsv(hsv.x, hsv.y + tintChange, hsv.z);
}
