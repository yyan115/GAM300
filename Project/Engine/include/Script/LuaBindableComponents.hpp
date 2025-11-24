#pragma once

// ============================================================================
// Math types
// ============================================================================
#include "Math/Vector3D.hpp"
#include "Transform/Quaternion.hpp"

// ============================================================================
// Core ECS Components
// ============================================================================
#include "Transform/TransformComponent.hpp"
#include "ECS/NameComponent.hpp"

// ============================================================================
// Animation Components
// ============================================================================
#include "Animation/AnimationComponent.hpp"

// ============================================================================
// AI/Brain Components
// ============================================================================
#include "Game AI/BrainComponent.hpp"

// ============================================================================
// Camera Component
// ============================================================================
#include "Graphics/Camera/CameraComponent.hpp"

// ============================================================================
// Light Components
// ============================================================================
#include "Graphics/Lights/LightComponent.hpp"

// ============================================================================
// Rendering Components
// ============================================================================
#include "Graphics/Particle/ParticleComponent.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"

// ============================================================================
// Physics Components
// ============================================================================
#include "Physics/ColliderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"


// ============================================================================
// CharacterController Components
// ============================================================================
#include "Physics/Kinematics/CharacterControllerComponent.hpp"


// ============================================================================
// Audio Components
// ============================================================================
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"