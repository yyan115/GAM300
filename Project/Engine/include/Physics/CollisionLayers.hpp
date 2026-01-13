#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer CHARACTER = 2;
    static constexpr JPH::ObjectLayer SENSOR = 3;
    static constexpr JPH::ObjectLayer DEBRIS = 4;
    static constexpr JPH::uint        COUNT = 5;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING{ 0 };
    static constexpr JPH::BroadPhaseLayer MOVING{ 1 };
    static constexpr JPH::BroadPhaseLayer CHARACTER{ 2 };

    static constexpr JPH::uint            COUNT = 3; // a plain count (uint)
}