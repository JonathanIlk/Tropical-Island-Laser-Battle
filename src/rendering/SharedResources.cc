#include "SharedResources.hh"
#include <glow/data/ColorSpace.hh>
#include <glow/objects.hh>
#include <glow-extras/geometry/Quad.hh>

void SharedResources::init() {
    colorPaletteTex = glow::Texture2D::createFromFile("../data/textures/ColorPaletteGrid.png", glow::ColorSpace::sRGB);
    logo = glow::Texture2D::createFromFile("../data/textures/Logo.png", glow::ColorSpace::Linear);

    simple = glow::Program::createFromFile("../data/shaders/simple");
    auto flatInstancedFSh = glow::Shader::createFromFile(
        GL_FRAGMENT_SHADER, "../data/shaders/flat/flat_instanced.fsh"
    ), flatInstancedVSh = glow::Shader::createFromFile(
        GL_VERTEX_SHADER, "../data/shaders/flat/flat_instanced.vsh"
    ), flatWindyVSh = glow::Shader::createFromFile(
        GL_VERTEX_SHADER, "../data/shaders/flat/flat_windy.vsh"
    );
    flatInstanced = glow::Program::create({flatInstancedVSh, flatInstancedFSh});
    flatWindy = glow::Program::create({flatWindyVSh, flatInstancedFSh});
    sprite = glow::Program::createFromFile("../data/shaders/ui/screen_sprite");

    mParrotMesh.loadMesh("../data/meshes/parrot/parrot.dae", ANIM_PARROT_IDLE);
    mParrotMesh.addAnimation("../data/meshes/parrot/parrot_AnimStartFlying.dae", ANIM_PARROT_START_FLY);
    mParrotMesh.addAnimation("../data/meshes/parrot/parrot_AnimFlyCircles.dae", ANIM_PARROT_FLY);
    mParrotMesh.addAnimation("../data/meshes/parrot/parrot_AnimFlyStartSequence.dae", ANIM_STARTSEQUENCE);

    std::vector<tg::pos3> markerVerts;
    markerVerts.reserve(4);
    markerVerts.push_back({0, 0, 0});
    for (int i = 0; i < 3; ++i) {
        auto [sin, cos] = tg::sin_cos(tg::angle::from_degree(-120 * i));
        markerVerts.push_back({sin / 3, 1, cos / 3});
    }
    tetrahedronMarker = glow::VertexArray::create(
        glow::ArrayBuffer::create("aPosition", markerVerts),
        glow::ElementArrayBuffer::create({
            0, 1, 2,    0, 2, 3,    0, 3, 1,    3, 2, 1
        }),
        GL_TRIANGLES
    );
    spriteQuad = glow::geometry::Quad<tg::pos2>().generate();
}
