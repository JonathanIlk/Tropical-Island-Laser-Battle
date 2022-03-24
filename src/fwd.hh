// SPDX-License-Identifier: MIT
#pragma once

class Game;
struct MainRenderPass;
struct SharedResources;
struct SimpleMesh;

namespace Combat {
    struct MobileUnit;
    struct Humanoid;
    struct HumanoidPos;
    struct HumanoidRenderInfo;
    class System;
}

namespace Demo {
    struct Animation;
    class System;
}

namespace ECS {
    class Editor;
    struct Rigid;
    struct ECS;
    struct Snapshot;
}

namespace Effects {
    struct ScatterLaser;
    class System;
}

namespace MeshViz {
    struct Instance;
    class System;
}

namespace NavMesh {
    struct Instance;
    struct RouteRequest;
    class System;
}

namespace Obstacle {
    struct Obstruction;
    class System;
    struct Type;
}

namespace Terrain {
    struct Instance;
    struct Rendering;
    class System;
}

namespace Water {
    struct Instance;
    class System;
}

namespace SkyBox
{
    struct Instance;
    class System;
}

namespace WorldFluff {
    class System;
    struct Type;
}

namespace StartSequence {
    struct Instance;
    class System;
}

namespace SpriteRenderer {
    struct Instance;
    class System;
}

namespace RiggedMesh {
    class Instance;
    class Data;
    class System;
}

namespace Parrot {
    struct Instance;
    class System;
}
