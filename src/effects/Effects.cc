// SPDX-License-Identifier: MIT
#include "Effects.hh"
#include <random>

#include <pcg_random.hpp>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/VertexArray.hh>

#include <Game.hh>
#include <MathUtil.hh>
#include <Util.hh>
#include <ECS/Misc.hh>
#include <util/ImGui.hh>
#include <util/SphericalDistributions.hh>

using namespace Effects;

void ScatterLaser::Params::updateUI() {
    ImGui::InputFloat("radius", &radius);
    ImGui::InputFloat("scatter radius", &radius);
    if (radius < 0) {radius = 0.f;}
    ImGui::InputFloat("ray width", &rayWidth);
    if (rayWidth < 0) {rayWidth = 0.f;}
    ImGui::InputFloat("fizzle density", &fizzleDensity);
    if (fizzleDensity < 0) {fizzleDensity = 0.f;}
    ImGui::InputFloat("fizzle drift", &drift);
    if (drift < 0) {drift = 0.f;}
    int i = numRays;
    if (ImGui::InputInt("#rays", &i)) {
        if (i < 0) {numRays = 0;}
        numRays = unsigned(i);
    }
    Util::editColor("ray color 1", color);
    Util::editColor("ray color 2", color2);
    ImGui::InputFloat4("ray timing", rayTiming.data());
    ImGui::InputFloat4("fizzle timing", fizzleTiming.data());
    for (auto i : Util::IntRange(3)) {
        if (rayTiming[i + 1] < rayTiming[i]) {rayTiming[i + 1] = rayTiming[i];}
        if (fizzleTiming[i + 1] < fizzleTiming[i]) {fizzleTiming[i + 1] = fizzleTiming[i];}
    }
}

System::System(Game &game) : mECS{game.mECS} {
    mLineSpriteVertices = glow::ArrayBuffer::create("aSpritePos", std::vector<tg::pos2> {
        {0, -1}, {0, 1}, {1, -1}, {1, 1}
    });
    mPointSpriteVertices = glow::ArrayBuffer::create("aSpritePos", std::vector<tg::pos2> {
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
    });
    mRayShader = glow::Program::createFromFiles({
        "../data/shaders/effects/linesprite.vsh", "../data/shaders/effects/laser.fsh"
    });
    mFizzleShader = glow::Program::createFromFile("../data/shaders/effects/fizzle");
}

void System::renderMain(MainRenderPass &pass) {
    {
        auto sh = mRayShader->use();
        sh["uViewProj"] = pass.viewProjMatrix;
        sh["uCamPos"] = pass.cameraPosition;

        for (auto &pair : mECS.scatterLasers) {
            auto &[id, laser] = pair;
            const auto &params = laser.params;
            auto &timing = params.rayTiming;

            float time = pass.snap->worldTime - laser.startTime;
            if (time <= timing[0] || time >= timing[3]) {continue;}
            sh["uPickID"] = id;
            auto color = params.color, rimColor = params.color2;
            auto alpha = time < timing[2]
                ? std::clamp((time - timing[0]) / (timing[1] - timing[0]), 0.f, 1.f)
                : 1 - (time - timing[2]) / (timing[3] - timing[2]);
            color.a *= alpha; rimColor.a *= alpha;
            sh["uColor"] = color;
            sh["uColor2"] = rimColor;
            glow::info() << "laser rays " << time << " " << color.a;
            laser.rayVAO->bind().draw(params.numRays);
        }
    }
    {
        auto sh = mFizzleShader->use();
        sh["uViewProj"] = pass.viewProjMatrix;
        sh["uCamPos"] = pass.cameraPosition;

        for (auto &pair : mECS.scatterLasers) {
            auto &[id, laser] = pair;
            const auto &params = laser.params;
            auto &timing = params.fizzleTiming;

            float localTime = pass.snap->worldTime - laser.startTime;
            if (localTime <= timing[0] || localTime >= timing[3]) {continue;}
            sh["uPickID"] = id;
            auto color = params.color;
            color.a *= std::clamp((localTime - timing[0]) / (timing[1] - timing[0]), 0.f, 1.f);
            sh["uColor"] = color;
            sh["uColor2"] = params.color2;
            sh["uSize"] = params.rayWidth;
            auto up = tg::vec3(pass.viewMatrix.row(1));
            sh["uUp"] = up;
            sh["uTime"] = localTime;
            auto fadeSlope = -1.f / (timing[3] - timing[2]);
            sh["uFadeSlope"] = fadeSlope;
            auto fadeBase = -timing[3] * fadeSlope;
            sh["uFadeBase"] = fadeBase;
            glow::info() << "laser fizzle " << localTime << " " << color.a << " " << (localTime * fadeSlope + fadeBase);
            laser.fizzleVAO->bind().draw(laser.numFizzle);
        }
    }
}

void System::cleanup(double time) {
    std::vector<ECS::entity> toDelete;
    for (auto &pair : mECS.scatterLasers) {
        auto &[id, laser] = pair;
        const auto &params = laser.params;

        float localTime = time - laser.startTime;
        if (localTime < params.fizzleTiming[3] || localTime < params.rayTiming[3]) {continue;}
        toDelete.push_back(id);
    }
    for (auto id : toDelete) {mECS.deleteEntity(id);}
}


void System::spawnScatterLaser(const tg::segment3 &seg, const ScatterLaser::Params &params) {
    if (params.numRays <= 0) {return;}

    pcg32 rng(std::seed_seq {uint64_t(mECS.simSnap->worldTime * 1000)});

    auto discOrient = Util::fromToRotation({0, 0, 1}, seg.pos1 - seg.pos0);
    auto mat = tg::mat4x3(ECS::Rigid {seg.pos1, discOrient});
    UnitDiscDistribution distr;

    struct RayData {
        float width;
        tg::pos3 start, end;
    };
    std::vector<RayData> rays;
    for (auto i : Util::IntRange(params.numRays)) {
        auto discPos = params.radius * distr(rng);
        rays.push_back({
            params.rayWidth,
            seg.pos0,
            tg::pos3(mat * tg::vec4(discPos.x, discPos.y, 0, 1))
        });
    }
    auto rayData = glow::ArrayBuffer::create({
        {&RayData::width, "aWidth"},
        {&RayData::start, "aStart"},
        {&RayData::end, "aEnd"},
    }, rays);
    rayData->setDivisor(1);
    auto rayVAO = glow::VertexArray::create({
        mLineSpriteVertices, std::move(rayData)
    }, nullptr, GL_TRIANGLE_STRIP);

    std::uniform_real_distribution<float> uniFloat {0.f, Util::afterOne<float>};
    std::uniform_int_distribution<unsigned> rayChoice {0, params.numRays - 1};
    UnitSphereDistribution sphere;

    struct FizzleData {
        tg::pos3 center;
        tg::vec3 drift;
        float timeOffset;
    };
    std::vector<FizzleData> fizzleParticles;
    auto fizzleSpread = params.fizzleTiming[2] - params.fizzleTiming[1];
    size_t numParticles = tg::distance(seg.pos0, seg.pos1) * params.fizzleDensity;
    glow::info() << numParticles << " fizzle particles";
    for (auto i : Util::IntRange(numParticles)) {
        auto param = uniFloat(rng);
        auto &ray = rays[rayChoice(rng)];
        auto timeOffset = uniFloat(rng) * fizzleSpread;
        auto thisDrift = sphere(rng) * params.drift;
        fizzleParticles.push_back({
            tg::lerp(ray.start, ray.end, param) - thisDrift * params.fizzleTiming[1],
            thisDrift,
            timeOffset
        });
    }
    auto fizzleData = glow::ArrayBuffer::create({
        {&FizzleData::center, "aCenter"},
        {&FizzleData::drift, "aDrift"},
        {&FizzleData::timeOffset, "aTimeOffset"}
    }, fizzleParticles);
    fizzleData->setDivisor(1);
    auto fizzleVAO = glow::VertexArray::create({
        mPointSpriteVertices, std::move(fizzleData)
    }, nullptr, GL_TRIANGLE_STRIP);

    auto ent = mECS.newEntity();
    mECS.scatterLasers.emplace(ent, ScatterLaser {
        mECS.simSnap->worldTime, seg, numParticles, params,
        std::move(rayVAO), std::move(fizzleVAO)
    });
}
