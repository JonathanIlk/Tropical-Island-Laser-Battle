// SPDX-License-Identifier: MIT
#include "AnimatorManager.hh"

std::list<std::shared_ptr<AbstractAnimator>> AnimatorManager::activeAnimators;

void AnimatorManager::updateAllAnimators(float deltaSeconds) {
    for (const auto& anim : activeAnimators)
    {
        anim->update(deltaSeconds);
    }
}

void AnimatorManager::start(std::shared_ptr<AbstractAnimator> animator) {
    animator->reset();
    activeAnimators.emplace_back(std::move(animator));
}

void AnimatorManager::stop(std::shared_ptr<AbstractAnimator> animator) {
    animator->reset();
    activeAnimators.remove(animator);
}
