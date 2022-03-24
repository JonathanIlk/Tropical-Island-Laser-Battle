// SPDX-License-Identifier: MIT
#pragma once
#include <animation/rigged/RiggedMesh.hh>
#include <glow/fwd.hh>

/// only for *shared* shaders/programs: remove entries as soon as only one
/// system/tool uses them
struct SharedResources
{
    glow::SharedProgram simple, flatInstanced, flatWindy, sprite;
    glow::SharedTexture2D colorPaletteTex, logo;
    glow::SharedVertexArray tetrahedronMarker, spriteQuad;

    RiggedMesh::Data mParrotMesh;
    const std::string ANIM_PARROT_IDLE = "IDLE";
    const std::string ANIM_PARROT_START_FLY = "START_FLY";
    const std::string ANIM_PARROT_FLY = "FLY";
    const std::string ANIM_STARTSEQUENCE = "STARTSEQUENCE";

    void init();
};
