// SPDX-License-Identifier: MIT
#include "SkyBox.hh"
#include <glow/objects/ElementArrayBuffer.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/objects/VertexArray.hh>

#include <ColorUtil.h>
#include <ECS/Join.hh>
#include <Mesh3D.hh>
#include <glow-extras/geometry/UVSphere.hh>
#include <glow/common/scoped_gl.hh>
#include "Terrain.hh"
#include "TerrainMaterial.hh"

using namespace SkyBox;

Instance::Instance(const Terrain::Instance &terrain) {
    skyBoxTransformation = tg::mat4::identity * terrain.center.x * 0.99;
    skyBoxTransformation[3] = tg::vec4(terrain.center.x, 0, terrain.center.y, 1);
    sandColor = ColorUtil::hsv(TerrainMaterial::table[TerrainMaterial::Sand].hsv);
}

void System::renderMain(MainRenderPass &pass)
{
    GLOW_SCOPED(disable, GL_CULL_FACE);
    GLOW_SCOPED(cullFace, GL_FRONT);

    mShaderSkyBox->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto shader = mShaderSkyBox->use();

    tg::mat4 view4x4(pass.viewMatrix);
    view4x4[3][3] = 1.0;

    pass.applyCommons(shader);
    pass.applyTime(shader);
    shader["uView"] = view4x4;
    shader["uProj"] = pass.projMatrix;
    shader["uNormalsTex"] = mLowPolyNormalsTex;
    shader["uNormals2Tex"] = mLowPolyNormals2Tex;
    shader["uTime"] = float(pass.snap->worldTime);

    for (auto &&tup : ECS::Join(pass.snap->rigids, mTerrains, mSkyBoxes))
    {
        auto &[wo, terrain, skyBox,  id] = tup;
        auto va = mVao->bind();
        shader["uModel"] = wo.transform_mat() * skyBox.skyBoxTransformation;
        shader["uPickID"] = id;
        shader["uWaterLevel"] = terrain.waterLevel;
        shader["uSandColor"] = skyBox.sandColor;
        shader["uPickID"] = id;
        va.draw();
    }
}

System::System(ECS::ECS &ecs) : mSkyBoxes{ecs.skyBoxes}, mTerrains{ecs.terrains} {
    Mesh3D cubeMesh;
    cubeMesh.loadFromFile("../data/meshes/cube.obj", false, false /* do not interpolate tangents for cubes */);
    mVao = cubeMesh.createVertexArray(); // upload to gpu

    mShaderSkyBox = glow::Program::createFromFile("../data/shaders/skybox/skybox");
    mLowPolyNormalsTex = glow::Texture2D::createFromFile("../data/textures/LowPolyNormals.png", glow::ColorSpace::Linear);
    mLowPolyNormals2Tex = glow::Texture2D::createFromFile("../data/textures/LowPolyNormals2.png", glow::ColorSpace::Linear);
    mLowPolyNormalsTex->bind().setWrap(GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT);
}
