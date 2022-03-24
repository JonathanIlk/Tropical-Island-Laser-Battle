// SPDX-License-Identifier: MIT
#include "Water.hh"
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/objects/VertexArray.hh>

#include <ECS/Join.hh>
#include "Terrain.hh"

using namespace Water;

Instance::Instance(const Terrain::Instance &terrain, tg::isize2 fbSize) {
    uint32_t nsegs = terrain.segmentsAmount;
    std::vector<uint32_t> indices;
    indices.reserve(6 * (nsegs - 1) * (nsegs - 1));
    for (uint32_t x = 0; x < nsegs - 1; ++x) {
        for (uint32_t z = 0; z < nsegs - 1; ++z) {
            indices.push_back(x * nsegs + z);
            indices.push_back(x * nsegs + z + 1);
            indices.push_back(x * nsegs + z + nsegs + 1);
            indices.push_back(x * nsegs + z);
            indices.push_back(x * nsegs + z + nsegs + 1);
            indices.push_back(x * nsegs + z + nsegs);
        }
    }
    this->vao = glow::VertexArray::create(
        std::vector<glow::SharedArrayBuffer>(),
        glow::ElementArrayBuffer::create(indices),
        GL_TRIANGLES
    );

    // render targets are half-size, rounded up
    fbSize += 1;
    fbSize /= 2;
    refractColor = glow::TextureRectangle::create(fbSize, GL_R11F_G11F_B10F);
    refractDepth = glow::TextureRectangle::create(fbSize, GL_DEPTH_COMPONENT32F);
    fbRefract = glow::Framebuffer::create();
    {
        auto boundFb = fbRefract->bind();
        boundFb.attachColor("fColor", refractColor);
        boundFb.attachDepth(refractDepth);
        boundFb.checkComplete();
    }

    reflectColor = glow::TextureRectangle::create(fbSize, GL_R11F_G11F_B10F);
    reflectDepth = glow::TextureRectangle::create(fbSize, GL_DEPTH_COMPONENT32F);
    fbReflect = glow::Framebuffer::create();
    {
        auto boundFb = fbReflect->bind();
        boundFb.attachColor("fColor", reflectColor);
        boundFb.attachDepth(reflectDepth);
        boundFb.checkComplete();
    }
}

void Instance::clearFramebuffers(tg::color3 bgColor) {
    refractColor->clear(GL_RGB, GL_FLOAT, tg::data_ptr(bgColor));
    reflectColor->clear(GL_RGB, GL_FLOAT, tg::data_ptr(bgColor));
    float const depthVal = 1.f;
    refractDepth->clear(GL_DEPTH_COMPONENT, GL_FLOAT, &depthVal);
    reflectDepth->clear(GL_DEPTH_COMPONENT, GL_FLOAT, &depthVal);
}

System::System(ECS::ECS &ecs) : mWaters{ecs.waters}, mTerrains{ecs.terrains} {
    mShaderWater = glow::Program::createFromFile("../data/shaders/water");
}

void System::renderMain(MainRenderPass &pass)
{
    mShaderWater->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto shader = mShaderWater->use();
    pass.applyCommons(shader);
    shader["uViewPortSize"] = tg::vec2(pass.viewPortSize);

    for (auto &&tup : ECS::Join(pass.snap->rigids, mTerrains, mWaters)) {
        auto &[wo, terrain, water,  id] = tup;
        auto va = water.vao->bind();
        shader["uModel"] = wo.transform_mat();
        shader["uTerrainRadius"] = (terrain.segmentSize * terrain.segmentsAmount) / 2;
        shader["uPickID"] = id;
        shader["uRefractTex"] = water.refractColor;
        shader["uReflectTex"] = water.reflectColor;
        shader["uRows"] = terrain.segmentsAmount;
        shader["uSegmentSize"] = terrain.segmentSize;
        // FIXME: after a few hours this will have precision problems because
        // GLSL uses 32-bit floats. a real solution would have a well-known period
        // and do an fmod before rounding
        shader["uTime"] = float(water.animationSpeed * pass.snap->worldTime);
        shader["uWaveHeight"] = water.waveHeight;
        shader["uWaterLevel"] = terrain.waterLevel;
        va.draw();
    }
}

void System::resize(tg::isize2 screenSize) {
    auto texSize = (screenSize + 1) / 2;  // round up to prevent 0x0
    for (auto &pair : mWaters) {
        auto &inst = pair.second;
        for (auto &t : {
            inst.refractColor, inst.refractDepth,
            inst.reflectColor, inst.reflectDepth
        }) {
            t->bind().resize(texSize);
        }
    }
}

tg::mat4 System::calculateReflectionMatrix(tg::vec4 plane) {
    tg::vec3 normal = tg::vec3(plane);
    float pDotV = -plane.w;
    // Reflectionmatrix taken from: http://web.cse.ohio-state.edu/~shen.94/781/Site/Slides_files/reflection.pdf
    auto reflectionMatrix = tg::mat4();
    reflectionMatrix.set_row(0, tg::vec4(1 - 2 * normal.x * normal.x, -2 * normal.x * normal.y, -2 * normal.x * normal.z, 2 * pDotV * normal.x));
    reflectionMatrix.set_row(1, tg::vec4(-2*normal.x*normal.y, 1-2*normal.y*normal.y, -2*normal.y*normal.z, 2*pDotV*normal.y));
    reflectionMatrix.set_row(2, tg::vec4(-2*normal.x*normal.z, -2*normal.y*normal.z, 1-2*normal.z*normal.z, 2*pDotV*normal.z));
    reflectionMatrix.set_row(3, tg::vec4(0, 0, 0, 1));
    return reflectionMatrix;
}
