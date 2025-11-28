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
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "ECS/SiblingIndexComponent.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "Hierarchy/ChildrenComponent.hpp"

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
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Graphics/DebugDraw/DebugDrawComponent.hpp"

// ============================================================================
// Physics Components
// ============================================================================
#include "Physics/ColliderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"

// ============================================================================
// Audio Components
// ============================================================================
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"

// ============================================================================
// UI Components
// ============================================================================
#include "UI/Button/ButtonComponent.hpp"