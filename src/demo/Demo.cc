// SPDX-License-Identifier: MIT
#include "Demo.hh"
#include <array>
#include <memory>

#include <typed-geometry/tg.hh>
#include <typed-geometry/feature/bezier.hh> // bezier curves for procgen
#include <glow/common/log.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Shader.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow-extras/geometry/UVSphere.hh>
#include <imgui/imgui.h>

#include <MathUtil.hh>
#include <Mesh3D.hh>
#include <SimpleMesh.hh>
#include <ECS/Join.hh>
#include <ECS/Misc.hh>
#include <rendering/MainRenderPass.hh>

using namespace Demo;

static std::unique_ptr<Mesh3D> buildProceduralMesh(int seed) {
    // random number generator
    tg::rng rng;
    rng.seed(seed);

    auto mesh = std::make_unique<Mesh3D>();

    auto curve_segments = 256;
    auto circle_segments = 16;
    auto radius = 0.8f;
    auto texture_scale = 2 * tg::pi_scalar<float> * radius; // how "big" 0..1 of the texture is

    // a local helper function to make a control point
    auto make_control_point = [&] {
        // a direction in typed-geometry (e.g. tg::dir3) is supposed to always be unit-length
        auto dir = tg::uniform<tg::dir3>(rng);
        auto radius = uniform(rng, 3.0f, 10.0f);
        // typed-geometry distinguishes between positions (e.g. tg::pos3) and vectors (e.g. tg::vec3)
        // note that vec + vec -> vec, pos + vec -> pos, but pos + pos -> error, as adding positions
        // does not make sense geometrically.
        // for component-wise math computations (without position or vector semantics), you can use tg::comp
        // however, explicitely casting between the types is almost always possible

        // here we explicitely turn a vector (dir * scalar yields a vector) into a position
        auto pos = tg::pos3(dir * radius);
        // move procedural mesh downwards
        pos.y = uniform(rng, 3.f, -1.f);

        // note: if you want to debug variable values, you can use e.g. "glow::info() << pos;"

        return pos;
    };

    // create bezier curve
    auto curve = tg::make_bezier( //
        tg::pos3(-5, 0, -5),      //
        make_control_point(),     //
        make_control_point(),     //
        make_control_point(),     //
        make_control_point(),     //
        tg::pos3(5, 0, 5));

    // create per-vertex attributes that we copy to per-halfedge ones later
    auto normal = mesh->vertices().make_attribute<tg::vec3>();
    auto tangent = mesh->vertices().make_attribute<tg::vec3>();
    auto curve_coords = mesh->vertices().make_attribute<tg::pos2>();
    auto curve_length_at = mesh->vertices().make_attribute<float>();

    // we create a generalized cylinder
    // a in [0,1] is coordinate along curve
    // b in [0,1] is coordinate along circle
    auto curve_length = 0.0f;
    auto last_pos = curve[0.0f];
    for (auto x = 0; x <= curve_segments; ++x)
    {
        // get point on curve
        auto a = x / float(curve_segments); // in [0,1]
        auto center = curve[a];             // evaluate bezier at a

        // accumulate estimated curve length
        curve_length += tg::distance(last_pos, center);
        last_pos = center;

        // extrude cylinder
        for (auto y = 0; y < circle_segments; ++y)
        {
            auto b = y / float(circle_segments); // in [0,1) (wraparound)

            // compute local coordinate system
            auto curve_dir = tangent_at(curve, a); // direction is tangent of curve
            auto n0 = normal_at(curve, a);         // normal and binormal are two perpendicular dirs
            auto n1 = binormal_at(curve, a);

            auto angle = 360_deg * b;
            auto [sina, cosa] = tg::sin_cos(angle); // get sin and cos

            // start from center, go to radius of cylinder
            auto circle_dir = n0 * sina + n1 * cosa;
            auto pos = center + radius * circle_dir;

            // create vertices
            auto v = mesh->vertices().add();
            mesh->position[v] = pos;

            // compute attributes
            tangent[v] = curve_dir; // tangent is direction of texture U
            normal[v] = normalize(circle_dir);
            curve_coords[v] = {a, b};
            curve_length_at[v] = curve_length; // later needed for texture coordinates
        }
    }

    // create faces (quads are ok here, they are triangulated in Mesh3D::createVertexArray
    auto wraps_around = mesh->faces().make_attribute(false);
    for (auto x = 0; x < curve_segments; ++x)
        for (auto y = 0; y < circle_segments; ++y)
        {
            auto vertex_at = [&](int ix, int iy) {
                // we know in which order we created vertices
                // thus we can compute index directly
                // NOTE: we have to wrap around iy as needed
                return mesh->vertices()[ix * circle_segments + (iy % circle_segments)];
            };

            // collect quad vertices
            auto v00 = vertex_at(x + 0, y + 0);
            auto v01 = vertex_at(x + 0, y + 1);
            auto v10 = vertex_at(x + 1, y + 0);
            auto v11 = vertex_at(x + 1, y + 1);

            // build quad
            auto f = mesh->faces().add(v00, v10, v11, v01);

            // flag if this face has wrapped around vertices
            if (y == circle_segments - 1)
                wraps_around[f] = true;
        }

    // copy per-vertex attributes to per-halfedge ones
    for (auto h : mesh->halfedges())
    {
        if (h.is_boundary())
            continue; // boundary halfedges don't generated faces

        auto v = h.vertex_to();
        mesh->tangent[h] = tangent[v];
        mesh->normal[h] = normal[v];

        // compute texture coordinates
        // these have to be per-halfedge because otherwise the cylinder warp-around would cause wrong interpolation
        auto cc = curve_coords[v];
        auto length = curve_length_at[v];

        // when the circle wraps around, b becomes 0 again and we see a wrong interpolation
        if (cc.y == 0 && wraps_around[h.face()])
            cc.y = 1; // .. so we just set it to 1

        // texture coordinates are derived from x/y but scaled properly
        mesh->texCoord[h] = {length / texture_scale, cc.y * 2 * tg::pi_scalar<float> * radius / texture_scale};
    }

    return mesh;
}

System::System(ECS::ECS &ecs) : mECS{ecs} {
    // color textures are usually sRGB and data textures (like normal maps) Linear
    mTexAlbedo = glow::Texture2D::createFromFile("../data/textures/concrete.albedo.jpg", glow::ColorSpace::sRGB);
    mTexNormal = glow::Texture2D::createFromFile("../data/textures/concrete.normal.jpg", glow::ColorSpace::Linear);
    mTexARM = glow::Texture2D::createFromFile("../data/textures/concrete.arm.jpg", glow::ColorSpace::Linear);
    auto vsh = glow::Shader::createFromFile(GL_VERTEX_SHADER, "../data/shaders/object.vsh");
    auto fsh = glow::Shader::createFromFile(GL_FRAGMENT_SHADER, "../data/shaders/object.fsh");
    auto fshSimple = glow::Shader::createFromFile(GL_FRAGMENT_SHADER, "../data/shaders/object_simple.fsh");
    mShaderObject = glow::Program::create({vsh, fsh});
    mShaderObjectSimple = glow::Program::create({vsh, fshSimple});

    mVaoSphere = glow::geometry::make_uv_sphere(64, 32);
    // cube.obj contains a cube with normals, tangents, and texture coordinates
    Mesh3D cubeMesh;
    cubeMesh.loadFromFile("../data/meshes/cube.obj", false, false /* do not interpolate tangents for cubes */);
    mVaoCube = cubeMesh.createVertexArray(); // upload to gpu
}

void System::addScene(int seed, tg::pos3 basePos, tg::quat baseRot) {
    ECS::entity ent = mECS.newEntity();
    mECS.staticRigids.emplace(ent, ECS::Rigid(basePos, baseRot));
    mECS.simpleMeshes.emplace(ent, SimpleMesh(
        buildProceduralMesh(seed)->createVertexArray(),
        mTexAlbedo,
        mTexNormal,
        mTexARM
    ));

    auto rot = tg::mat3(baseRot);
    basePos += rot * tg::vec3(0, -3, -2);
    auto xVec = rot * tg::vec3(-3, .5f, -3);
    auto zVec = rot * tg::vec3(3, .5f, -3);
    std::array<uint32_t, 5> colorScheme {0x003049, 0xD62828, 0xF77F00, 0xFCBF49, 0xEAE2B7}; // just hex colors "from the web"
    for (auto x = 0; x < 8; ++x) {
        for (auto z = 0; z < 9; ++z) {
            ent = mECS.newEntity();
            tg::pos3 pos = basePos + x * xVec + z * zVec;
            auto &sm = mECS.simpleMeshes.emplace(ent, SimpleMesh(
                (x % 2 == 0 && z % 3 == 0) ? mVaoSphere : mVaoCube,
                mTexAlbedo, mTexNormal, mTexARM
            )).first->second;
            // alpha is stored in the 8 least significant bits
            float alpha = tg::min(1.f, float(x + z) / 7.f);
            sm.albedoBias = colorScheme[(x * z) % colorScheme.size()] << 8 | (uint32_t(alpha * 255.f) & 0xFF);

            Animation anim {pos};
            float animSpeed = (x * z + 1.f) * 0.25f;
            if (x % 3 == 0 && z % 2 == 0) {
                anim.bounceVec = tg::vec3(0, 1, 0);
                anim.bounceSpeed = animSpeed;
            } else {
                anim.angularVelocity = tg::vec3(0, animSpeed, 0);
            }
            mECS.demoAnim.emplace(ent, std::move(anim));
        }
    }
}

void System::renderMain(MainRenderPass &pass) {
    mShaderObject->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto sh = mShaderObject->use();
    pass.applyCommons(sh);

    for (auto &&tup : ECS::Join(pass.snap->rigids, mECS.simpleMeshes)) {
        auto &[wo, sm, id] = tup;
        sh["uModel"] = wo.transform_mat();
        sh["uPickingID"] = id;
        sh["uAlbedoBias"] = Util::unpackSRGBA(sm.albedoBias);
        sh["uTexAlbedo"] = sm.texAlbedo;
        sh["uTexNormal"] = sm.texNormal;
        sh["uTexARM"] = sm.texARM;

        sm.vao->bind().draw();
    }
}

void System::extrapolate(ECS::Snapshot &next) {
    for (auto &&tup : mECS.demoAnim) {
        auto &[id, anim] = tup;
        auto pi = 180_deg .radians();
        auto translation = anim.basePosition + sin(float(
            std::fmod(next.worldTime * anim.bounceSpeed, pi)
        )) * anim.bounceVec;
        auto angv = tg::length(anim.angularVelocity);
        auto rotation = angv == 0 ? tg::quat::identity : tg::normalize(tg::quat::from_axis_angle(
            tg::dir3(anim.angularVelocity / angv),
            tg::angle32::from_radians(std::fmod(next.worldTime * angv, pi))
        ));
        next.rigids[id] = {translation, rotation};
    }
}
