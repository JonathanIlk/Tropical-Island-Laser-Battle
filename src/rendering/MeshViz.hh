// SPDX-License-Identifier: MIT
#pragma once
#include <typed-geometry/tg-lean.hh>
#include <polymesh/fwd.hh>
#include <glow/fwd.hh>

#include <ECS.hh>

namespace MeshViz {

struct Instance {
    glow::SharedVertexArray vaFaces, vaBoundaries;

    Instance(const pm::Mesh &mesh, const pm::vertex_attribute<tg::pos3> &pos, float pushOut);
};

class System {
    ECS::ComponentMap<Instance> &mVizMeshes;
    glow::SharedProgram mShaderMeshViz;

public:
    System(ECS::ECS &);
    void renderMain(MainRenderPass &);
};

}
