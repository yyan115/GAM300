#pragma once

// Force names method to exist when Jolt was built with profiling
#ifndef JPH_PROFILE_ENABLED
#define JPH_PROFILE_ENABLED 1
#endif

#include <array>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
//#include <Jolt/Physics/Collision/CollisionFilter.h>
#include "CollisionLayers.hpp"

// BroadPhase mapping (object-layer -> broadphase-layer)
// Use std::array to avoid Jolt allocations before RegisterDefaultAllocator()
class MyBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    MyBroadPhaseLayerInterface() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::MOVING;
        mObjectToBroadPhase[Layers::DEBRIS] = BroadPhaseLayers::MOVING;
    }

    ~MyBroadPhaseLayerInterface() override = default;

    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::COUNT; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::COUNT);
        return mObjectToBroadPhase[static_cast<size_t>(inLayer)];
    }

//#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
//    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
//        switch (inLayer.GetValue()) {
//        case 0: return "NON_MOVING";
//        case 1: return "MOVING";
//        default: return "INVALID";
//        }
//    }
//#endif

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const = 0;
#endif

private:
    std::array<JPH::BroadPhaseLayer, Layers::COUNT> mObjectToBroadPhase{};
};

// Broadphase culling
class MyObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    ~MyObjectVsBroadPhaseLayerFilter() override = default;
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        if (layer == Layers::NON_MOVING) return bp == BroadPhaseLayers::MOVING;
        return bp == BroadPhaseLayers::MOVING; // MOVING/SENSOR/DEBRIS test only vs MOVING
    }
};

// Narrowphase pair filter
class MyObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    ~MyObjectLayerPairFilter() override = default;
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (a == Layers::NON_MOVING && b == Layers::NON_MOVING) return false; // optional perf
        if (a == Layers::DEBRIS && b == Layers::DEBRIS)         return false; // optional
        return true; // allow sensors to overlap (use Body::SetIsSensor(true) to avoid constraints)
    }
};
