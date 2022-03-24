#include "RiggedMesh.hh"
#include <animation/AnimatorManager.hh>
#include <glow-extras/assimp/Importer.hh>
#include <glow/common/log.hh>
#include <ECS/Join.hh>
#include <ECS/Misc.hh>
#include <Game.hh>
#include <glow/objects.hh>

using namespace RiggedMesh;

void Data::loadMesh(const std::string& filename, const std::string& loadWithAnimName) {

    auto importer = glow::assimp::Importer();
    importer.setLoadBones(true);
    importer.setFlipUVs(true);
    importer.setLoadAnimationName(loadWithAnimName);
    auto meshData = importer.loadData(filename);

    mVao = meshData->createVertexArray();
    rootBoneName = meshData->rootBoneName;
    bones = meshData->bones;
    addAnimationFromData(meshData);
}

void Data::addAnimation(const std::string& filename, const std::string& loadWithAnimName) {
    auto importer = glow::assimp::Importer();
    importer.setLoadAnimationName(loadWithAnimName);
    importer.setLoadBones(true);
    auto meshData = importer.loadData(filename);
    addAnimationFromData(meshData);
}

void Data::addAnimationFromData(glow::assimp::SharedMeshData& data) {
    auto anim = data->animations.begin();
    auto animName = anim->first;
    auto riggedAnim = RiggedAnimation::fromLoadedData(anim->second);
    animations[animName] = std::make_shared<RiggedAnimation>(riggedAnim);
}

Instance::Instance(Data* data, const std::string& startAnimName): meshData(data)
{
    animator = std::make_shared<RiggedAnimator>(RiggedAnimator(meshData->animations[startAnimName], meshData->bones, meshData->rootBoneName));
    animator->loop = true;
}

System::System(Game& game) : mGame(game)
{
    mShader = glow::Program::createFromFiles({"../data/shaders/flat/flat.fsh", "../data/shaders/rigged/rigged_mesh.vsh"});
    // Disable warnings since Program::getUniform does not work for arrays, which leads to the warning despite the data being present.
    mShader->setWarnOnUnchangedUniforms(false);
    mTexAlbedo = mGame.mSharedResources.colorPaletteTex;
}

void System::renderMain(MainRenderPass& pass) {

    for (auto &&tup : ECS::Join(mGame.mECS.riggedRigids, mGame.mECS.riggedMeshes))
    {
        auto &[wo, instance, id] = tup;
        renderInstance(instance, pass, wo.transform_mat());
    }
}

void System::renderInstance(RiggedMesh::Instance& instance, MainRenderPass& pass, tg::mat4x3 modelMat) const
{
    const BonesKeyFrame& frame = instance.animator->currentState();
    tg::vec4 translations[MAX_BONES];
    tg::vec4 rotations[MAX_BONES];
    for (std::size_t i = 0; i < frame.mValues.size(); i++) {
        auto& val = frame.mValues[i];
        translations[i] = tg::vec4(val.mPosition.value);
        rotations[i] = tg::vec4(val.mRotation.value);
    }
    {
        mShader->setUniformBuffer("uLighting", pass.lightingUniforms);
        auto sh = mShader->use();
        pass.applyCommons(sh);
        sh["uModel"] = modelMat;
        sh["uTexAlbedo"] = mTexAlbedo;
        sh["uPickID"] = 10203u;
        sh["uBonesRotations"] = rotations;
        sh["uBonesTranslations"] = translations;

        instance.meshData->mVao->bind().draw();
    }
}
