#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer SENSOR = 2;
    static constexpr JPH::ObjectLayer DEBRIS = 3;
    static constexpr JPH::uint        COUNT = 4;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING{ 0 };
    static constexpr JPH::BroadPhaseLayer MOVING{ 1 };
    static constexpr JPH::uint            COUNT = 2; // a plain count (uint)
}
