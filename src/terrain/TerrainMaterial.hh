// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <typed-geometry/tg-lean.hh>

struct TerrainMaterial
{
    enum ID : short {
        Rock = 0,
        Grass,
        Sand,
        NumMaterials
    };
    ID id;
    tg::vec3 hsv;

    tg::color3 randomTint(uint32_t seed) const;

    static const std::array<TerrainMaterial, NumMaterials> table;
};
