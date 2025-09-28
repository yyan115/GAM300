#pragma once



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
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer)
        {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
        default:													JPH_ASSERT(false); return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    std::array<JPH::BroadPhaseLayer, Layers::COUNT> mObjectToBroadPhase{};
};

// Broadphase culling
class MyObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp) const override {
        switch (layer) {
        case Layers::NON_MOVING:
            return bp == BroadPhaseLayers::MOVING; // static vs moving only
        case Layers::MOVING:
        case Layers::SENSOR:
        case Layers::DEBRIS:
            return bp == BroadPhaseLayers::MOVING || bp == BroadPhaseLayers::NON_MOVING; // allow floor
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
        if (a == Layers::NON_MOVING && b == Layers::NON_MOVING) return false; // optional perf
        if (a == Layers::DEBRIS && b == Layers::DEBRIS)         return false; // optional
        return true; // allow sensors to overlap (use Body::SetIsSensor(true) to avoid constraints)
    }
};
