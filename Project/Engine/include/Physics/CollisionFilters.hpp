#pragma once
#include <array>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include "CollisionLayers.hpp"

// BroadPhase mapping (object-layer -> broadphase-layer)
class MyBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    MyBroadPhaseLayerInterface() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::CHARACTER] = BroadPhaseLayers::CHARACTER;
        mObjectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::DEBRIS] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::NAV_GROUND] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::NAV_OBSTACLE] = BroadPhaseLayers::NON_MOVING;
    }

    ~MyBroadPhaseLayerInterface() override = default;

    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::COUNT; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::COUNT);
        return mObjectToBroadPhase[static_cast<size_t>(inLayer)];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::CHARACTER:  return "CHARACTER";
        default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif

private:
    std::array<JPH::BroadPhaseLayer, Layers::COUNT> mObjectToBroadPhase{};
};

// Broadphase culling
class MyObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        switch (layer) {
        case Layers::NON_MOVING:
        case Layers::NAV_GROUND:
        case Layers::NAV_OBSTACLE:
            return bp == BroadPhaseLayers::MOVING || bp == BroadPhaseLayers::CHARACTER || bp == BroadPhaseLayers::NON_MOVING;

        case Layers::MOVING:
        case Layers::SENSOR:
        case Layers::DEBRIS:
            return bp == BroadPhaseLayers::MOVING ||
                bp == BroadPhaseLayers::NON_MOVING ||
                bp == BroadPhaseLayers::CHARACTER;  // Allow collision with characters

        case Layers::CHARACTER:
            return bp == BroadPhaseLayers::MOVING || bp == BroadPhaseLayers::NON_MOVING || bp == BroadPhaseLayers::CHARACTER;

        default:
            return false;
        }
    }
};

// Narrowphase pair filter
class MyObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    ~MyObjectLayerPairFilter() override = default;

    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        // Static vs Static: both immovable, collision is pointless
        if (a == Layers::NON_MOVING && b == Layers::NON_MOVING) return false;

        // Debris vs Debris: skip
        if (a == Layers::DEBRIS && b == Layers::DEBRIS) return false;

        // Sensor vs Sensor: trigger volumes don't need to detect each other
        if (a == Layers::SENSOR && b == Layers::SENSOR) return false;

        // Nav layers vs Nav layers: navigation geometry never collides with itself
        if ((a == Layers::NAV_GROUND || a == Layers::NAV_OBSTACLE) &&
            (b == Layers::NAV_GROUND || b == Layers::NAV_OBSTACLE)) return false;

        // Nav layers vs static geometry: both immovable
        if ((a == Layers::NAV_GROUND || a == Layers::NAV_OBSTACLE) && b == Layers::NON_MOVING) return false;
        if (a == Layers::NON_MOVING && (b == Layers::NAV_GROUND || b == Layers::NAV_OBSTACLE)) return false;

        return true;
    }
};