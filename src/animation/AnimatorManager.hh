// SPDX-License-Identifier: MIT
#pragma once

#include <list>
#include <memory>
#include "Animator.hh"

class AnimatorManager
{
    static std::list<std::shared_ptr<AbstractAnimator>> activeAnimators;
public:
    static void updateAllAnimators(float deltaSeconds);
    static void start(std::shared_ptr<AbstractAnimator> animator);
    static void stop(std::shared_ptr<AbstractAnimator> animator);
};

