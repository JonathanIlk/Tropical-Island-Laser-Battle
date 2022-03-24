// SPDX-License-Identifier: MIT
#pragma once
#include <glow/fwd.hh>

#include "ECS.hh"

struct SimpleMesh {
    // mesh
    glow::SharedVertexArray vao;

    // material (textures)
    glow::SharedTexture2D texAlbedo;
    glow::SharedTexture2D texNormal;
    glow::SharedTexture2D texARM; // Ambient Occlusion + Roughness + Metallic packed into R/G/B
    uint32_t albedoBias = 0;          // packed RGBA color added to albedo

    SimpleMesh(glow::SharedVertexArray vertices, glow::SharedTexture2D albedo, glow::SharedTexture2D normal, glow::SharedTexture2D arm) : vao{vertices}, texAlbedo{albedo}, texNormal{normal}, texARM{arm} {}
};
