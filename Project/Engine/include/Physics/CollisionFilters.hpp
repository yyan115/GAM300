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
        mObjectToBroadPhase[Layers::CHARACTER] = BroadPhaseLayers::CHARACTER;  // Add this
        mObjectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::DEBRIS] = BroadPhaseLayers::MOVING;
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
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::CHARACTER:  return "CHARACTER";  // Add this
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
            return bp == BroadPhaseLayers::MOVING || bp == BroadPhaseLayers::CHARACTER || bp == BroadPhaseLayers::NON_MOVING;

        case Layers::MOVING:
        case Layers::SENSOR:
        case Layers::DEBRIS:
            return bp == BroadPhaseLayers::MOVING ||
                bp == BroadPhaseLayers::NON_MOVING ||
                bp == BroadPhaseLayers::CHARACTER;  // Allow collision with characters

        case Layers::CHARACTER:
            return bp == BroadPhaseLayers::MOVING || bp == BroadPhaseLayers::NON_MOVING;  // Add character collision rules

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
        //if (a == Layers::NON_MOVING && b == Layers::NON_MOVING) return false;
        if (a == Layers::DEBRIS && b == Layers::DEBRIS) return false;

        // Optional: Prevent characters from colliding with each other
        // if (a == Layers::CHARACTER && b == Layers::CHARACTER) return false;

        return true;
    }
};