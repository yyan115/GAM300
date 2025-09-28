#include "pch.h"
#include "Physics/CollisionFilters.hpp"

// MyBroadPhaseLayerInterface implementation
MyBroadPhaseLayerInterface::MyBroadPhaseLayerInterface() {
    // Map object layers to broad phase layers
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
}

JPH::uint MyBroadPhaseLayerInterface::GetNumBroadPhaseLayers() const {
    return BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer MyBroadPhaseLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
}

const char* MyBroadPhaseLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        default: JPH_ASSERT(false); return "INVALID";
    }
}

// MyObjectVsBroadPhaseLayerFilter implementation
bool MyObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
    switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// MyObjectLayerPairFilter implementation
bool MyObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const {
    switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
    }
}