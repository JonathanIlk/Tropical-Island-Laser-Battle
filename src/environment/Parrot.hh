// SPDX-License-Identifier: MIT
#pragma once
#include "fwd.hh"

namespace Parrot
{
    struct Instance {
        bool wasFrightened = false;
    };

    class System {
        float parrotFrightenDistance = 10;

        Game& mGame;
        SharedResources& mSharedResources;
    public:
        System(Game& game);
        void behaviorUpdate();
    };
};
