// SPDX-License-Identifier: MIT
#include "MeshViz.hh"
#include <polymesh/Mesh.hh>
#include <glow/common/log.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/VertexArray.hh>
#include <imgui/imgui.h>

#include <ECS/Join.hh>
#include <ECS/Misc.hh>
#include <rendering/MainRenderPass.hh>

MeshViz::Instance::Instance(const pm::Mesh &mesh, const pm::vertex_attribute<tg::pos3> &position, float pushOut) {
    std::vector<tg::pos3> corners;
    std::vector<tg::vec3> corner_normals;
    size_t nAllHE = mesh.all_halfedges().size();
    corners.reserve(nAllHE);
    corner_normals.reserve(nAllHE);
    /* include removed halfedges to have corresponding indexes.
    In practice the mesh should be compact anyway */
    size_t limit = 5;
    for (auto h : mesh.all_halfedges()) {
        if (h.is_removed()) {
            corners.push_back({0.0, 0.0, 0.0});
        } else {
            auto prev = position[h.vertex_from()];
            auto cur = position[h.vertex_to()];
            auto next = position[h.next().vertex_to()];
            auto pos = tg::lerp(tg::lerp(prev, next, 0.5), cur, 0.9);
            auto normal = tg::normalize(tg::cross(cur - next, cur - prev));
            corners.push_back(pos + pushOut * normal);
            corner_normals.push_back(normal);
        }
    }
    for (size_t i = 0; i < 10; ++i) {
        auto &pos = corners[i];
        auto &normal = corner_normals[i];
        glow::info() << pos.x << " " << pos.y << " " << pos.z
            << " | " << normal.x << " " << normal.y << " " << normal.z;
    }
    std::vector<uint32_t> index_faces;
    auto position_buf = glow::ArrayBuffer::create("aPosition", corners);
    auto normal_buf = glow::ArrayBuffer::create("aNormal", corner_normals);
    index_faces.reserve(3 * (mesh.halfedges().size() - 2 * mesh.faces().size()));
    for (auto f : mesh.faces()) {
        auto iter = f.halfedges().begin();
        uint32_t first_idx = iter.handle.idx.value;
        ++iter;
        uint32_t last_idx = iter.handle.idx.value;
        while (true) {  /* "fan" triangulation */
            ++iter;
            if (iter == f.halfedges().end()) {break;}
            uint32_t idx = iter.handle.idx.value;
            index_faces.push_back(first_idx);
            index_faces.push_back(last_idx);
            index_faces.push_back(idx);
            last_idx = idx;
        }
    }
    this->vaFaces = glow::VertexArray::create(
        {position_buf, normal_buf},
        glow::ElementArrayBuffer::create(index_faces),
        GL_TRIANGLES
    );
    std::vector<uint32_t> index_boundaries;
    for (auto h : mesh.halfedges()) {
        index_boundaries.push_back(h.prev().idx.value);
        index_boundaries.push_back(h.opposite().prev().idx.value);
        index_boundaries.push_back(h.idx.value);
    }
    for (auto f : mesh.vertices()) {
        auto iter = f.outgoing_halfedges().begin();
        uint32_t first_idx = iter.handle.opposite().idx.value;
        ++iter;
        uint32_t last_idx = iter.handle.opposite().idx.value;
        while (true) {  /* "fan" triangulation */
            ++iter;
            if (iter == f.outgoing_halfedges().end()) {break;}
            uint32_t idx = iter.handle.opposite().idx.value;
            index_boundaries.push_back(first_idx);
            index_boundaries.push_back(last_idx);
            index_boundaries.push_back(idx);
            last_idx = idx;
        }
    }
    this->vaBoundaries = glow::VertexArray::create(
        {position_buf, normal_buf},
        glow::ElementArrayBuffer::create(index_boundaries),
        GL_TRIANGLES
    );
}

MeshViz::System::System(ECS::ECS &ecs) : mVizMeshes{ecs.vizMeshes} {
    mShaderMeshViz = glow::Program::createFromFile("../data/shaders/mesh_viz");
}

void MeshViz::System::renderMain(MainRenderPass &pass) {
    auto shader = mShaderMeshViz->use();
    shader["uViewProj"] = pass.viewProjMatrix;
    for (auto &&tup : ECS::Join(pass.snap->rigids, mVizMeshes)) {
        auto &[wo, vm, id] = tup;
        shader["uPickID"] = id;
        shader["uModel"] = wo.transform_mat();
        {
            auto va = vm.vaFaces->bind();
            shader["uLuminance"] = 0.2f;
            va.draw();
        }
        {
            auto va = vm.vaBoundaries->bind();
            shader["uLuminance"] = 0.01f;
            va.draw();
        }
    }
}
