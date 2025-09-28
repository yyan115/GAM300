#pragma once
#include "JoltInclude.hpp"
#include "../Engine.h"

// BroadPhaseLayerInterface implementation
class ENGINE_API MyBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    MyBroadPhaseLayerInterface();

    virtual JPH::uint GetNumBroadPhaseLayers() const override;
    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// ObjectVsBroadPhaseLayerFilter implementation
class ENGINE_API MyObjectVsBroadPhaseLayerFilter : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
};

// ObjectLayerPairFilter implementation
class ENGINE_API MyObjectLayerPairFilter : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override;
};