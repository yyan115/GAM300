#pragma once
#undef JPH_OBJECT_STREAM
#undef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
#undef JPH_PROFILE_ENABLED
#define JPH_OBJECT_STREAM 0
#define JPH_FLOATING_POINT_EXCEPTIONS_ENABLED 0
#define JPH_PROFILE_ENABLED 0
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>


//Shapes
#include <Jolt/Physics/Collision/Shape/BoxShape.h>