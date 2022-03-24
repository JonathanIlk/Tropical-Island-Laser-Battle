#include "Parrot.hh"
#include <ECS/Join.hh>
#include <combat/Combat.hh>
#include <ECS/Misc.hh>
#include <Game.hh>

using namespace Parrot;

System::System(Game& game) : mGame{game}, mSharedResources(game.mSharedResources) {

}
void System::behaviorUpdate() {
    RiggedMesh::Data& parrotMesh = mGame.mSharedResources.mParrotMesh;
    for (auto &&tup : ECS::Join(mGame.mECS.riggedRigids, mGame.mECS.riggedMeshes, mGame.mECS.parrots))
    {
        auto &[wo, instance, parrot, id] = tup;
        if(parrot.wasFrightened) {
            continue;
        }
        // FIXME: Implement KDTree for faster proximity check, but this will suffice for now
        const tg::pos2 parrotPos(wo.translation.x, wo.translation.z);
        tg::pos2 humanoidPos;
        for (auto const& [humEnt, hum] : mGame.mECS.simSnap->humanoids)
        {
            const tg::pos3& humTrans = hum.base.translation;
            humanoidPos.x = humTrans.x;
            humanoidPos.y = humTrans.z;
            if (tg::distance(parrotPos, humanoidPos) < parrotFrightenDistance)
            {
                instance.animator->setNewAnimation(instance.meshData->animations[mSharedResources.ANIM_PARROT_START_FLY]);
                instance.animator->enqueAnimation(mSharedResources.mParrotMesh.animations[mSharedResources.ANIM_PARROT_FLY]);
                parrot.wasFrightened = true;
            }
        }
    }
}
