// SPDX-License-Identifier: MIT
#include "Combat.hh"
#include <algorithm>
#include <cinttypes>

#include <typed-geometry/tg-std.hh>
#include <glow/common/scoped_gl.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/objects/VertexArray.hh>
#include <imgui/imgui.h>

#include <Game.hh>
#include <Util.hh>
#include <MathUtil.hh>
#include <combat/CommandTool.hh>
#include <combat/Walking.hh>
#include <ECS/Join.hh>
#include <effects/Effects.hh>
#include <navmesh/NavMesh.hh>
#include <obstacles/Obstacle.hh>
#include <rendering/MainRenderPass.hh>
#include <util/SphericalDistributions.hh>

using namespace Combat;

void System::spawnSquad(NavMesh::Instance &nav, unsigned units, float radius, std::mt19937 &rng) {
    std::uniform_int_distribution faceDistr {0, nav.mesh->all_faces().size() - 1};
    std::uniform_real_distribution snorm {-1.f, Util::afterOne<float>};
    MobileUnit mobTempl;
    mobTempl.radius = .5f;
    mobTempl.cruiseSpeed = 10.f;
    mobTempl.acceleration = 7.f;
    auto unitHeight = 1.8f, spawnDist = mobTempl.radius * 1.5f;
    auto &ecs = mGame.mECS;

    while (true) {  // FIXME: what to do if we can't ever find a suitable place?
        auto face = nav.mesh->handle_of(pm::face_index(faceDistr(rng)));
        tg::vec3 sum; size_t nVert = 0;
        for (auto v : face.vertices()) {
            sum += tg::vec3(nav.worldPos[v]);
            nVert += 1;
        }
        tg::pos3 center(sum / nVert);
        std::vector<ECS::entity> obstacles, selected;
        ecs.obstructions.visit([center, radius] (const tg::aabb3 &a, int) {
            auto closest = tg::min(tg::max(center, a.min), a.max);
            return tg::length(closest - center) <= radius;
        }, [&] (const Obstacle::Obstruction &obst) {
            if (tg::distance(center, tg::lerp(obst.aabb.min, obst.aabb.max, .5f) ) <= radius) {
                obstacles.push_back(obst.id);
            }
            return true;
        });
        if (obstacles.size() < units) {
            glow::info() << "rejecting " << center;
            continue;
        }
        std::sample(obstacles.begin(), obstacles.end(), std::back_inserter(selected), units, rng);
        for (auto id : selected) {
            auto join = ECS::Join(ecs.obstacles, ecs.instancedRigids);
            auto iter = join.find(id);
            TG_ASSERT(iter != join.end());
            auto [type, rig, obstId] = *iter;
            auto mat = tg::mat4x3(rig);
            auto up = tg::dir3(mat[1]);
            auto directionsTried = 0;
            auto baseOrient = Util::upForwardOrientation(up, rig.translation - center);
            while (true) {  // FIXME: again, what if we never find anything?
                directionsTried += 1;
                TG_ASSERT(directionsTried < 100);

                auto angle = tg::asin(snorm(rng));
                auto orient = baseOrient * tg::quat::from_axis_angle({0, 1, 0}, angle.radians() < 0 ? 90_deg + angle : angle - 90_deg);
                auto offsetDir = orient * tg::dir3(angle < 0_deg ? 1 : -1, 0, 0);
                auto dir = tg::conjugate(rig.rotation) * offsetDir;
                auto dist = 0.f;
                type.collisionMesh->vertexTree.visit([&dist, dir] (const tg::aabb3 &a, int) {
                    auto maxDist = dir.x * (dir.x >= 0.f ? a.max.x : a.min.x) + dir.y * (dir.y >= 0.f ? a.max.z : a.min.z);
                    return maxDist >= dist;
                }, [&dist, dir] (const tg::pos3 &p) {
                    dist = std::max(dist, dir.x * p.x + dir.y * p.z);
                    return true;
                });
                auto pos = rig.translation - offsetDir * (dist + spawnDist);
                tg::ray3 ray {pos + 2.f * up, -up};
                auto ints = nav.intersect(ray);
                if (!ints) {
                    glow::info() << "no intersection found" << pos << rig.translation;
                    continue;
                }
                pos = ray[ints->second];
                glow::info() << "placing enemy at " << pos;

                auto ent = ecs.newEntity();
                ecs.editables.emplace(ent, &*ecs.combatSys);
                auto &hum = ecs.humanoids[ent];
                hum.allegiance = 1;
                hum.scatterLaserParams.color = {1.0, 0.0, 0.0, 1};
                hum.scatterLaserParams.color2 = {1.0, 0.0, 0.22, 1};
                auto &mob = ecs.mobileUnits.emplace(ent, mobTempl).first->second;
                mob.heightVector = up *  unitHeight;
                ecs.simSnap->humanoids.emplace(ent, MovementContext {ecs, hum, mob, nav}.restPos({pos, orient})
                );
                break;
            }
        }
        break;
    }
}

System::System(Game& game) : mGame(game) {
    mGoodGuyMesh.loadMesh("../data/meshes/good_guy.dae", "");
    mBadGuyMesh.loadMesh("../data/meshes/bad_guy.dae", "");
    mSimpleShader = game.mSharedResources.simple;
    mRiggedShader = glow::Program::createFromFiles({"../data/shaders/flat/flat.fsh", "../data/shaders/rigged/rigged_mesh.vsh"});
    mGaugeShader = glow::Program::createFromFile("../data/shaders/ui/hpindicator");
    Mesh3D shotgunMesh;
    shotgunMesh.loadFromFile("../data/meshes/shotgun.obj", true, false);
    mShotgunVao = shotgunMesh.createVertexArray();

    // Disable warnings since Program::getUniform does not work for arrays, which leads to the warning despite the data being present.
    mRiggedShader->setWarnOnUnchangedUniforms(false);

    struct GaugeVertex {
        tg::pos3 aPosition;
        float aValue;
    };
    std::vector<GaugeVertex> gauge;
    for (auto i : Util::IntRange(51)) {
        auto val = i / 50.f;
        auto [sin, cos] = tg::sin_cos(120_deg * val - 60_deg);
        gauge.insert(gauge.end(), {
            {{sin, 1.f, cos}, val},
            {{sin, 0.f, cos}, val}
        });
    }
    mHPGaugeVao = glow::VertexArray::create(
        glow::ArrayBuffer::create({
            {&GaugeVertex::aPosition, "aPosition"},
            {&GaugeVertex::aValue, "aValue"}
        }, gauge), GL_TRIANGLE_STRIP
    );
    mPathABO = glow::ArrayBuffer::create("aPosition", std::vector<tg::pos3>{});
    mPathVao = glow::VertexArray::create(mPathABO, GL_TRIANGLE_STRIP);
}

void System::editorUI(ECS::entity ent) {
    auto join = ECS::Join(mGame.mECS.humanoids, mGame.mECS.mobileUnits);
    auto iter = join.find(ent);
    if (iter == join.end()) {
        ImGui::Text("Entity %" PRIu32 " is associated with the Humanoid editor, but is not a humanoid", ent);
        return;
    }
    auto [humanoid, mob, id] = *iter;
    auto &hum = *humanoid.visual;
    if (ImGui::Button("Give command")) {
        mGame.mActiveTool = std::make_unique<CommandTool>(mGame, tg::acos(humanoid.attackCos), mob.radius, humanoid.attackRange);
    }
    ImGui::InputFloat("Attack range", &humanoid.attackRange);
    ImGui::InputInt("Allegiance", &humanoid.allegiance);
    ImGui::InputInt("HP", &humanoid.hp);
    bool update = false;
    if (ImGui::TreeNodeEx("Pelvis", ImGuiTreeNodeFlags_DefaultOpen)) {
        update |= ImGui::SliderFloat("width", &hum.pelvicSize.width, .25f, .45f);
        update |= ImGui::SliderFloat("height", &hum.pelvicSize.height, .15f, .30f);
        update |= ImGui::SliderFloat("depth", &hum.pelvicSize.depth, .15f, .30f);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Feet", ImGuiTreeNodeFlags_DefaultOpen)) {
        update |= ImGui::SliderFloat("ankle x", &hum.ankleJoint.x, .08f, .15f);
        update |= ImGui::SliderFloat("ankle height", &humanoid.ankleHeight, .08f, .15f);
        float ankleDepth = hum.footSize.depth - hum.ankleJoint.y;
        if (ImGui::SliderFloat("ankle depth", &ankleDepth, .05f, .1f)) {
            hum.ankleJoint.y = hum.footSize.depth - ankleDepth;
            update = true;
        }
        update |= ImGui::SliderFloat("width", &hum.footSize.width, .08f, .15f);
        update |= ImGui::SliderFloat("height", &hum.footSize.height, .08f, .15f);
        update |= ImGui::SliderFloat("depth", &hum.footSize.depth, .2f, .3f);
        ImGui::TreePop();
    }
    if (update) {hum.update(humanoid);}
    if (ImGui::TreeNode("Laser")) {
        humanoid.scatterLaserParams.updateUI();
        ImGui::TreePop();
    }
}

static bool inCone(const tg::vec3 &distVec, float maxDist, const tg::dir3 &center, float maxCos) {
    auto dist = tg::length(distVec);
    if (dist > maxDist) {return false;}
    return dot(distVec, center) >= maxCos * dist;
}

void System::extrapolate(ECS::Snapshot &prev, ECS::Snapshot &next) {
    auto time = next.worldTime;
    for (auto &&tup : ECS::Join(mGame.mECS.humanoids, mGame.mECS.mobileUnits)) {
        auto &[hum, mob, id] = tup;

        if (hum.steps.empty()) {
            next.humanoids[id] = prev.humanoids[id];
            continue;
        }

        auto navIter = mGame.mECS.navMeshes.find(mob.nav);
        if (navIter == mGame.mECS.navMeshes.end()) {continue;}

        MovementContext {
            mGame.mECS, hum, mob, navIter->second
        }.interpolate(next.humanoids[id], time);
    }
    float dt = time - prev.worldTime;
    if (dt <= 0.f) {return;}  // don't divide by 0 when paused
    for (auto &&tup : ECS::Join(mGame.mECS.humanoids, next.humanoids)) {
        auto [hum, humpos, id] = tup;

        if (!hum.steps.empty() || hum.curTarget == ECS::INVALID) {continue;}
        auto humposIter = next.humanoids.find(hum.curTarget);
        if (humposIter == next.humanoids.end()) {continue;}
        auto humIter = mGame.mECS.humanoids.find(hum.curTarget);
        if (humIter == mGame.mECS.humanoids.end()) {continue;}
        auto bodyCenter = humposIter->second.upperBody * humIter->second.bodyCenter;

        auto gunCenter = humpos.upperBody * hum.gunCenter;
        auto fwd = humpos.base * tg::dir3(0, 0, -1);
        auto distVec = bodyCenter - gunCenter;
        if (!inCone(distVec, hum.attackRange, fwd, hum.attackCos)) {continue;}

        // we work in upperBody space for this computation
        auto gunFwd = humpos.gun.rotation * tg::dir3(0, 0, -1);
        auto aimDir = tg::conjugate(humpos.upperBody.rotation) * tg::normalize(distVec);

        auto angle = tg::angle_between(aimDir, gunFwd);
        auto param = std::clamp(hum.turningSpeed * dt / angle, 0.f, 1.f);
        humpos.gun.rotation = tg::slerp(humpos.gun.rotation, Util::forwardUpOrientation(aimDir, {0, 1, 0}), param);
        humpos.gun.translation = hum.gunCenter + humpos.gun.rotation * tg::vec3(0, 0, -hum.gunOffset);
        humpos.head.rotation = humpos.gun.rotation;
    }
}

bool System::lineOfSight(const tg::pos3 &pos, const tg::vec3 &distVec) const {
    auto dist = tg::length(distVec);
    if (!(dist > 0)) {return true;}
    tg::ray3 ray {
        pos, tg::dir3(distVec / dist)
    };
    auto obst = mGame.mECS.obstacleSys->rayCast(ray);
    if (obst) {
        glow::info() << "obstacle " << obst->first << " at " << obst->second;
    }
    if (obst && obst->second < dist) {return false;}

    auto terr = mGame.mECS.navMeshSys->intersect(ray);
    if (terr && std::get<2>(*terr) < dist) {return false;}

    // this could be extended to cover smokescreens or something

    return true;
}

void System::update(ECS::Snapshot &prev, ECS::Snapshot &next) {
    auto &humMap = mGame.mECS.humanoids;
    auto humanoids = ECS::Join(humMap, next.humanoids);
    std::vector<ECS::entity> kills;
    for (auto &&tup : humanoids) {
        auto [hum_, humpos, id] = tup;
        auto &hum = hum_;  // I love you too, C++ standard
        // don't aim while moving
        if (!hum.steps.empty()) {
            if (hum.curTarget != ECS::INVALID) {
                glow::info() << "unit " << id << " lost target " << hum.curTarget << " by moving";
            }
            hum.curTarget = ECS::INVALID;
            if (hum.steps.back().time > next.worldTime) {continue;}
            hum.steps.clear();
        }
        auto gunCenter = humpos.upperBody * hum.gunCenter;
        auto fwd = humpos.base * tg::dir3(0, 0, -1);
        if (hum.curTarget != ECS::INVALID && [&] () {
            auto humpos2 = next.humanoids.find(hum.curTarget);
            auto hum2 = humMap.find(hum.curTarget);
            if (humpos2 == next.humanoids.end()) {return true;}
            if (hum2 == humMap.end()) {return true;}
            auto bodyCenter = humpos2->second.upperBody * hum2->second.bodyCenter;
            auto distVec = bodyCenter - gunCenter;
            if (!inCone(
                distVec, hum.attackRange, fwd, hum.attackCos
            )) {return true;}
            return !lineOfSight(gunCenter, distVec);
        }()) {
            glow::info() << "unit " << id << " lost target " << hum.curTarget;
            hum.curTarget = ECS::INVALID;
        }
        if (hum.curTarget == ECS::INVALID) {
            ECS::entity bestEnt = ECS::INVALID;
            float bestDist = hum.attackRange;
            auto fwd = humpos.base * tg::dir3(0, 0, -1);
            for (auto &&tup : humanoids) {
                auto [hum2, humpos2, id2] = tup;
                if (hum2.allegiance == hum.allegiance) {
                    continue;  // don't target a friend
                }
                auto bodyCenter = humpos2.upperBody * hum2.bodyCenter;
                auto distVec = bodyCenter - gunCenter;
                if (!inCone(distVec, bestDist, fwd, hum.attackCos)) {continue;}
                if (!lineOfSight(gunCenter, distVec)) {continue;}
                bestEnt = id2;
            }
            if (bestEnt != ECS::INVALID) {
                glow::info() << "unit " << id << " acquired target " << bestEnt;
                hum.curTarget = bestEnt;
            } else {
                continue;  // can't aim without a target
            }
        }

        if (hum.shotReadyAt > next.worldTime) {continue;}

        auto humposIter = next.humanoids.find(hum.curTarget);
        auto humIter = mGame.mECS.humanoids.find(hum.curTarget);
        // the target loss/acquisition code above should ensure we only reach
        // this point with the target having both Humanoid and HumanoidPos
        TG_ASSERT(humposIter != next.humanoids.end());
        TG_ASSERT(humIter != mGame.mECS.humanoids.end());
        const auto &humpos2 = humposIter->second;
        auto &hum2 = humIter->second;

        auto bodyCenter = humpos2.upperBody * hum2.bodyCenter;
        auto distVec = bodyCenter - gunCenter;
        auto distDir = tg::normalize(distVec);
        auto gunFwd = (humpos.upperBody.rotation * humpos.gun.rotation) * tg::dir3(0, 0, -1);

        if (tg::dot(distDir, gunFwd) < tg::cos(6_deg)) {continue;}
        glow::info() << id <<  " shoots at at target " << hum.curTarget;
        auto muzzlePos = gunCenter + hum.gunOffset * gunFwd;
        mGame.mECS.effectsSys->spawnScatterLaser(
            {muzzlePos, bodyCenter}, hum.scatterLaserParams
        );
        if (hum2.hp > hum.attackDamage) {
            hum2.hp -= hum.attackDamage;
        } else if (hum2.hp > 0) {
            glow::info() << "kill confirmed";
            hum2.hp = 0;
            kills.push_back(hum.curTarget);
            hum.curTarget = ECS::INVALID;
        }
        hum.shotReadyAt = next.worldTime + hum.cooldown;
    }
    for (auto id : kills) {
        mGame.mECS.deleteEntity(id);
    }
}

void System::prepareRender(ECS::Snapshot &snap) {
    for (auto &&tup : ECS::Join(snap.humanoids, mGame.mECS.humanoids)) {
        auto &[pos, hum, id] = tup;
        auto hip = pos.upperBody * pos.hip, gun = pos.upperBody * pos.gun;
        auto up = pos.upperBody.rotation * tg::dir3(0, 1, 0);
        auto gunFwd = gun.rotation * tg::dir3(0, 0, -1);
        auto right = tg::cross(gunFwd, up);
        auto rightLen = tg::length(right);
        ECS::Rigid chest {
            pos.upperBody * tg::pos3(0, hum.shoulderHeight - hum.hipHeight, 0)
        };
        if (rightLen >= .1f) {
            right /= rightLen;
            auto gunFwdOrient = tg::quat::from_rotation_matrix(tg::mat3(right, tg::vec3(up), tg::cross(right, up)));
            chest.rotation = tg::slerp(hip.rotation, gunFwdOrient, .5f);
        } else {
            // we don't really handle very high or low angles of aiming, just use
            // something that won't be completely broken
            chest.rotation = hip.rotation;
        }
        auto rightHand = gun * hum.rightHandPos;
        auto cooldown = std::max(0.f, float(hum.shotReadyAt - snap.worldTime));
        auto leftHand = gun * hum.leftHandPos.interpolate(
            hum.pumpHandPos,
            std::clamp(cooldown > .25f ? -4 * cooldown + 2.f : 4 * cooldown, 0.f, 1.f)
        );
        snap.humRender.push_back({
            id, hum, hip, gun, chest, pos.upperBody * pos.head,
            {pos.legPos(0, hum), pos.legPos(1, hum)},
            {pos.armPos(0, chest, hum, rightHand), pos.armPos(1, chest, hum, leftHand)}
        });
    }
}

void System::renderMain(MainRenderPass &pass) {
    GLOW_SCOPED(enable, GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(0xffff);
    auto shader = mGame.mSharedResources.simple;
    shader->setUniformBuffer("uLighting", pass.lightingUniforms);
    auto sh = shader->use();
    pass.applyCommons(sh);
    sh["uAlbedo"] = tg::vec3(.2, .2, .2);
    sh["uARM"] = tg::vec3(1, .95, 0);
    sh["uEmission"] = tg::vec3::zero;


    for (auto &info : pass.snap->humRender) {
        sh["uPickID"] = info.id;
        auto &hum = info.hum;
        auto &vis = *hum.visual;
        auto& meshToDraw = hum.allegiance == 0 ? mGoodGuyMesh : mBadGuyMesh;

        auto& rootBone = meshToDraw.bones["CharacterArmature_Bone"];
        auto& chestBone = meshToDraw.bones["CharacterArmature_Chest"];
        auto& headBone = meshToDraw.bones["CharacterArmature_Head"];
        auto& shoulderRightBone = meshToDraw.bones["CharacterArmature_Shoulder1"];
        auto& shoulderLeftBone = meshToDraw.bones["CharacterArmature_Shoulder2"];
        auto& armUpperRightBone = meshToDraw.bones["CharacterArmature_arm_upper1"];
        auto& armUpperLeftBone = meshToDraw.bones["CharacterArmature_arm_upper2"];
        auto& armLowerRightBone = meshToDraw.bones["CharacterArmature_arm_lower1"];
        auto& armLowerLeftBone = meshToDraw.bones["CharacterArmature_arm_lower2"];
        auto& armHandRightBone = meshToDraw.bones["CharacterArmature_arm_hand1"];
        auto& armHandLeftBone = meshToDraw.bones["CharacterArmature_arm_hand2"];
        auto& legUpperRightBone = meshToDraw.bones["CharacterArmature_leg_upper1"];
        auto& legLowerRightBone = meshToDraw.bones["CharacterArmature_leg_lower2"];
        auto& legFootRightBone = meshToDraw.bones["CharacterArmature_leg_foot1"];
        auto& legUpperLeftBone = meshToDraw.bones["CharacterArmature_leg_upper3"];
        auto& legLowerLeftBone = meshToDraw.bones["CharacterArmature_leg_lower4"];
        auto& legFootLeftBone = meshToDraw.bones["CharacterArmature_leg_foot2"];

        sh["uModel"] = tg::mat4x3(info.gun * ECS::Rigid {{0, 0, .3f}});
        mShotgunVao->bind().draw();

        std::array<tg::vec4, 17> translations {}, rotations {};
        auto setBone = [&] (const ECS::Rigid &rig, size_t id, const tg::pos3 &base) {
            rotations[id] = tg::vec4(rig.rotation);
            translations[id] = tg::vec4(rig.translation - rig.rotation * (base - tg::vec3(0, hum.hipHeight, 0)));
        };
        setBone(info.hip, rootBone.id, {0, hum.hipHeight, 0});
        auto shoulderHeight = hum.shoulderHeight;
        setBone(info.chest, chestBone.id, {0, shoulderHeight, 0});
        setBone(info.head, headBone.id, {0, hum.axisHeight, 0});
        setBone(info.chest, shoulderLeftBone.id, {0, shoulderHeight, 0});
        setBone(info.chest, shoulderRightBone.id, {0, shoulderHeight, 0});
        setBone(info.chest, shoulderLeftBone.id, {0, shoulderHeight, 0});

        tg::pos3 shoulderRightBase ;
        float shoulderOffset = 0.2075f, elbowOffset = 0.585f, armOffset = 0.715f;
        auto &armPosRight = info.arms[0];
        setBone(armPosRight.upperArm, armUpperRightBone.id, {shoulderOffset, shoulderHeight, 0});
        setBone(armPosRight.lowerArm, armLowerRightBone.id, {elbowOffset, shoulderHeight, 0});
        setBone(armPosRight.hand, armHandRightBone.id, {armOffset, shoulderHeight, 0});

        auto &armPosLeft = info.arms[1];
        setBone(armPosLeft.upperArm, armUpperLeftBone.id, {-shoulderOffset, shoulderHeight, 0});
        setBone(armPosLeft.lowerArm, armLowerLeftBone.id, {-elbowOffset, shoulderHeight, 0});
        setBone(armPosLeft.hand, armHandLeftBone.id, {-armOffset, shoulderHeight, 0});


        auto &legPosRight = info.legs[0];
        setBone(legPosRight.thigh, legUpperRightBone.id, {.5f * hum.hipJointDist, hum.hipHeight, 0});
        setBone(legPosRight.lowerLeg, legLowerRightBone.id, {.5f * hum.hipJointDist, hum.kneeHeight, 0});
        setBone(legPosRight.foot, legFootRightBone.id, {.5f * hum.hipJointDist, 0, 0});

        auto &legPosLeft = info.legs[1];
        setBone(legPosLeft.thigh, legUpperLeftBone.id, {-.5f * hum.hipJointDist, hum.hipHeight, 0});
        setBone(legPosLeft.lowerLeg, legLowerLeftBone.id, {-.5f * hum.hipJointDist, hum.kneeHeight, 0});
        setBone(legPosLeft.foot, legFootLeftBone.id, {-.5f * hum.hipJointDist, 0, 0});

        {
            mRiggedShader->setUniformBuffer("uLighting", pass.lightingUniforms);
            auto shaderino = mRiggedShader->use();
            pass.applyCommons(shaderino);
            shaderino["uModel"] = tg::mat4x3::identity;
            shaderino["uTexAlbedo"] = mGame.mSharedResources.colorPaletteTex;
            shaderino["uPickID"] = info.id;
            shaderino["uBonesRotations"] = rotations;
            shaderino["uBonesTranslations"] = translations;

            meshToDraw.mVao->bind().draw();
        }
    }

    for (auto &&tup : ECS::Join(pass.snap->rigids, mGame.mECS.humanoids)) {
        auto &[pos, hum, id] = tup;
        sh["uPickID"] = id;
        auto mat = tg::mat4x3(pos), model = mat;
        auto &vis = *hum.visual;

        // draw the humanoid in T-Pose
        model[3] = mat * tg::vec4(0, hum.hipHeight, 0, 1);
        sh["uModel"] = model;
        vis.vaoHip->bind().draw();
        model[3] = mat * tg::vec4(0, hum.shoulderHeight, 0, 1);
        sh["uModel"] = model;
        vis.vaoChest->bind().draw();
        model[3] = mat * tg::vec4(0, hum.axisHeight, 0, 1);
        sh["uModel"] = model;
        vis.vaoHead->bind().draw();

        for (auto i : Util::IntRange(2)) {
            auto sign = i == 0 ? 1.f : -1.f;
            auto offset = sign * .5f * hum.hipJointDist;
            model[3] = mat * tg::vec4(offset, 0, 0, 1);
            auto &side = vis.side[i];
            sh["uModel"] = model;
            side.vaoFoot->bind().draw();
            model[3] = mat * tg::vec4(offset, hum.kneeHeight, 0, 1);
            sh["uModel"] = model;
            side.vaoLowerLeg->bind().draw();
            model[3] = mat * tg::vec4(offset, hum.hipHeight, 0, 1);
            sh["uModel"] = model;
            side.vaoThigh->bind().draw();
            model[3] = mat * tg::vec4(sign * .5f * hum.shoulderDist, hum.shoulderHeight, 0, 1);
            sh["uModel"] = model;
            side.vaoUpperArm->bind().draw();
            model[3] = mat * tg::vec4(sign * (.5f * hum.shoulderDist + hum.elbowPos), hum.shoulderHeight, 0, 1);
            sh["uModel"] = model;
            side.vaoLowerArm->bind().draw();
            model[3] = mat * tg::vec4(sign * (.5f * hum.shoulderDist + hum.armLength), hum.shoulderHeight, 0, 1);
            sh["uModel"] = model;
            side.vaoHand->bind().draw();
        }
        {
            std::array<tg::vec4, 17> translations, rotations;
            for (auto &xlat : translations) {xlat = {0, hum.hipHeight, 0, 1};}
            for (auto &rot : rotations) {rot = {0, 0, 0, 1};}
            mSimpleShader->setUniformBuffer("uLighting", pass.lightingUniforms);
            auto shaderino = mRiggedShader->use();
            pass.applyCommons(shaderino);
            shaderino["uModel"] = tg::mat4x3(pos);
            shaderino["uTexAlbedo"] = mGame.mSharedResources.colorPaletteTex;
            shaderino["uPickID"] = id;
            shaderino["uBonesRotations"] = rotations;
            shaderino["uBonesTranslations"] = translations;

            mGoodGuyMesh.mVao->bind().draw();
        }
    }
}

void System::prepareUI(ECS::Snapshot &snap) {
    auto pathHeight = .1f, pathWidth = .2f;
    mPathRanges.clear();
    std::vector<tg::pos3> paths;
    for (auto &&tup : ECS::Join(mGame.mECS.humanoids, snap.humanoids)) {
        auto [hum, pos, id] = tup;
        if (hum.allegiance != 0) {continue;}
        if (hum.steps.empty() || hum.steps.back().time < snap.worldTime) {continue;}
        mPathRanges.push_back(paths.size());
        paths.push_back(pos.base * tg::pos3(0, pathHeight, 0));
        // thanks to the check above, this should never return end()
        auto iter = std::upper_bound(
            hum.steps.begin(), hum.steps.end(),
            snap.worldTime, [] (double t, const Step &s) {return t < s.time;}
        );
        while (true) {
            auto &pos = iter->pos;
            ++iter;  // check if the next step is the end
            if (iter == hum.steps.end()) {break;}
            paths.push_back(pos.base * tg::pos3(pathWidth / 2, pathHeight, 0));
            paths.push_back(pos.base * tg::pos3(-pathWidth / 2, pathHeight, 0));
        }
        paths.push_back(hum.steps.back().pos.base * tg::pos3(0, pathHeight, 0));
    }
    mPathRanges.push_back(paths.size());
    mPathABO->bind().setData(paths);
}

void System::renderUI(MainRenderPass &pass) {
    {
        auto gaugeHeight = .2f;
        mGaugeShader->setUniformBuffer("uLighting", pass.lightingUniforms);
        auto sh = mGaugeShader->use();
        pass.applyCommons(sh);
        sh["uAlbedo"] = tg::color3(.2f, .2f, .2f);
        sh["uLeftColor"] = tg::color3(0.f, .8f, 0.f);
        sh["uRightColor"] = tg::color3(.8f, 0.f, 0.f);
        auto vao = mHPGaugeVao->bind();

        for (auto &&tup : ECS::Join(mGame.mECS.humanoids, mGame.mECS.mobileUnits, pass.snap->humanoids)) {
            auto [hum, mob, pos, id] = tup;
            if (hum.hp >= hum.maxHP) {continue;}
            auto up = pos.base.rotation * tg::vec3(0, gaugeHeight, 0);
            auto z = pass.cameraPosition - pos.base.translation;
            auto right = tg::cross(up, z);
            auto rightLen = tg::length(right);
            z = tg::cross(right, up);
            auto zLen = tg::length(z);
            if (rightLen == 0.f || zLen == 0.f) {continue;}
            auto mat = tg::mat4x3(
                right * (mob.radius / rightLen), up, z * (mob.radius / zLen),
                tg::vec3(pos.base.translation)
            );
            sh["uModel"] = mat;
            sh["uPickID"] = id;
            sh["uValue"] = float(hum.hp) / hum.maxHP;
            vao.draw();
        }
    }
    {
        mSimpleShader->setUniformBuffer("uLighting", pass.lightingUniforms);
        auto sh = mSimpleShader->use();
        pass.applyCommons(sh);
        sh["uModel"] = tg::mat4x3::identity;
        sh["uPickID"] = ECS::INVALID;
        sh["uAlbedo"] = tg::color3(.2f, .2f, .2f);
        sh["uARM"] = tg::vec3(1, .95, 0);
        sh["uEmission"] = tg::color3(0, .8f, 0);
        auto vao = mPathVao->bind();
        for (auto i : Util::IntRange(std::size_t(1), mPathRanges.size())) {
            vao.drawRange(mPathRanges[i - 1], mPathRanges[i]);
        }
    }
}
