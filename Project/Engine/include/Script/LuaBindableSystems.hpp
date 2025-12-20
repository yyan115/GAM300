#pragma once
#include "Math/Vector3D.hpp"
#include <tuple>

// ============================================================================
// INPUT SYSTEM WRAPPERS
// ============================================================================
#include "Input/Keys.h"
#include "Input/InputManager.hpp"

// Wrapper functions to convert int to enum for Input functions
namespace InputWrappers {
    inline bool GetKey(int key) {
        return InputManager::GetKey(static_cast<Input::Key>(key));
    }

    inline bool GetKeyDown(int key) {
        return InputManager::GetKeyDown(static_cast<Input::Key>(key));
    }

    inline bool GetMouseButton(int button) {
        return InputManager::GetMouseButton(static_cast<Input::MouseButton>(button));
    }

    inline bool GetMouseButtonDown(int button) {
        return InputManager::GetMouseButtonDown(static_cast<Input::MouseButton>(button));
    }
}

// ============================================================================
// PHYSICS SYSTEM WRAPPERS
// ============================================================================
#include "Physics/PhysicsSystem.hpp"
#include "ECS/ECSRegistry.hpp"

namespace PhysicsSystemWrappers {
    inline PhysicsSystem* g_PhysicsSystem = nullptr;

    inline JPH::PhysicsSystem* GetSystem() {
        if (!g_PhysicsSystem) {
            std::cerr << "[ERROR] PhysicsSystem not initialized in wrappers!" << std::endl;
            return nullptr;
        }
        return &g_PhysicsSystem->GetJoltSystem();
    }

    // Simple test function to verify Lua bindings work
    inline float TestBinding() {
        std::cout << "[Physics] TestBinding called from Lua!" << std::endl;
        return 42.0f;
    }

    // Raycast wrapper for Lua - returns distance to hit, or -1 if no hit
    // Usage: local distance = Physics.Raycast(originX, originY, originZ, dirX, dirY, dirZ, maxDistance)
    // Returns: distance (float, -1.0 if no hit)
    inline float Raycast(
        float originX, float originY, float originZ,
        float dirX, float dirY, float dirZ,
        float maxDistance)
    {
        if (!g_PhysicsSystem) {
            return -1.0f;
        }

        Vector3D origin(originX, originY, originZ);
        Vector3D direction(dirX, dirY, dirZ);

        auto result = g_PhysicsSystem->Raycast(origin, direction, maxDistance);
        return result.hit ? result.distance : -1.0f;
    }
}


// ============================================================================
// RIGIDBODY SYSTEM WRAPPERS
// ============================================================================
#include "Physics/RigidBodyComponent.hpp"

namespace RigidBodySystemWrappers {
    inline void AddForce(RigidBodyComponent& rigidbody, float x, float y, float z)
    {
        rigidbody.AddForce(Vector3D(x,y,z));
    }
    inline void AddTorque(RigidBodyComponent& rigidbody, float x, float y, float z)
    {
        rigidbody.AddTorque(Vector3D(x, y, z));
    }
    inline void AddImpulse(RigidBodyComponent& rigidbody, float x, float y, float z)
    {
        rigidbody.AddImpulse(Vector3D(x, y, z));
    }
}



// ============================================================================
// CHARACTER CONTROLLER WRAPPERS
// ============================================================================
#include "Physics/Kinematics/CharacterController.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Transform/TransformComponent.hpp"

namespace CharacterControllerWrappers {
    // Constructor wrapper
    inline CharacterController* Create() {
        JPH::PhysicsSystem* physicsSystem = PhysicsSystemWrappers::GetSystem();

        if (!physicsSystem) {
            std::cerr << "[ERROR] Cannot create CharacterController - PhysicsSystem unavailable!" << std::endl;
            return nullptr;
        }
        return new CharacterController(physicsSystem);
    }

    inline bool Initialise(CharacterController* controller,
        ColliderComponent* collider,
        Transform* transform) {

        if (controller && collider && transform) {
            return controller->Initialise(*collider, *transform);
        }
        return false;
    }

    inline void Update(CharacterController* controller, float deltaTime) {
        if (controller)
            controller->Update(deltaTime);
    }

    // Movement wrappers
    inline void Move(CharacterController* controller, float x, float y, float z) {
        if (controller)
            controller->Move(x, y, z);
    }

    inline void Jump(CharacterController* controller, float height) {
        if (controller)
            controller->Jump(height);
    }

    inline Vector3D GetPosition(CharacterController* controller) {
        if (controller)
            return controller->GetPosition();
        return Vector3D(0, 0, 0);
    }

    inline void SetVelocity(CharacterController* controller, float x, float y, float z) {
        if (controller)
            controller->SetVelocity(Vector3D(x, y, z));
    }

    inline Vector3D GetVelocity(CharacterController* controller) {
        if (controller)
            return controller->GetVelocity();
        return Vector3D(0, 0, 0);
    }

    inline bool IsGrounded(CharacterController* controller) {
        if (controller)
            return controller->IsGrounded();
        return false;
    }

    inline Vector3D GetGravity(CharacterController* controller) {
        if (controller)
            return controller->GetGravity();
        return Vector3D(0, -9.81f, 0);
    }

    inline void SetGravity(CharacterController* controller, float x, float y, float z) {
        if (controller)
            controller->SetGravity(Vector3D(x, y, z));
    }

    inline void Destroy(CharacterController* controller) {
        if (controller)
            delete controller;
    }
}

// ============================================================================
// TAG SYSTEM WRAPPERS
// ============================================================================
#include "ECS/TagManager.hpp"
#include "ECS/TagComponent.hpp"

namespace TagWrappers {
    // Get tag name by index
    inline std::string GetTagName(int index) {
        return TagManager::GetInstance().GetTagName(index);
    }
    
    // Get tag index by name
    inline int GetTagIndex(const std::string& name) {
        return TagManager::GetInstance().GetTagIndex(name);
    }
    
    // Get total number of tags
    inline int GetTagCount() {
        return TagManager::GetInstance().GetTagCount();
    }
    
    // Compare tag by name (for entity.tag == "Player" style comparisons)
    inline bool CompareTag(int tagIndex, const std::string& tagName) {
        int targetIndex = TagManager::GetInstance().GetTagIndex(tagName);
        return tagIndex == targetIndex && targetIndex != -1;
    }
    
    // Compare two tag names
    inline bool CompareTagNames(const std::string& tag1, const std::string& tag2) {
        return tag1 == tag2;
    }
}

// ============================================================================
// LAYER SYSTEM WRAPPERS
// ============================================================================
#include "ECS/LayerManager.hpp"
#include "ECS/LayerComponent.hpp"

namespace LayerWrappers {
    // Get layer name by index
    inline std::string GetLayerName(int index) {
        return LayerManager::GetInstance().GetLayerName(index);
    }
    
    // Get layer index by name (returns -1 if not found)
    inline int GetLayerIndex(const std::string& name) {
        return LayerManager::GetInstance().GetLayerIndex(name);
    }
    
    // Check if entity is in a specific layer by name
    inline bool IsInLayer(int layerIndex, const std::string& layerName) {
        int targetIndex = LayerManager::GetInstance().GetLayerIndex(layerName);
        return layerIndex == targetIndex && targetIndex != -1;
    }
    
    // Get layer mask from layer index
    inline int GetLayerMask(int layerIndex) {
        return 1 << layerIndex;
    }
    
    // Check if two layer masks intersect
    inline bool LayerMasksIntersect(int mask1, int mask2) {
        return (mask1 & mask2) != 0;
    }
}

// ============================================================================
// TIME SYSTEM WRAPPERS
// ============================================================================
#include "TimeManager.hpp"

namespace TimeWrappers {
    inline float GetDeltaTime() {
        return static_cast<float>(TimeManager::GetDeltaTime());
    }
    
    inline float GetFixedDeltaTime() {
        return static_cast<float>(TimeManager::GetFixedDeltaTime());
    }
    
    inline float GetFPS() {
        return static_cast<float>(TimeManager::GetFps());
    }
    
    // Time scale for slow-motion/fast-forward effects (static variable)
    static float s_timeScale = 1.0f;
    
    inline float GetTimeScale() {
        return s_timeScale;
    }
    
    inline void SetTimeScale(float scale) {
        s_timeScale = scale;
    }
    
    // Scaled delta time for gameplay (respects time scale)
    inline float GetScaledDeltaTime() {
        return static_cast<float>(TimeManager::GetDeltaTime()) * s_timeScale;
    }
}

// ============================================================================
// SCENE SYSTEM WRAPPERS
// ============================================================================
#include "Scene/SceneManager.hpp"

namespace SceneWrappers {
    inline void LoadScene(const std::string& scenePath) {
        SceneManager::GetInstance().LoadScene(scenePath);
    }
    
    inline std::string GetCurrentSceneName() {
        return SceneManager::GetInstance().GetSceneName();
    }
    
    inline std::string GetCurrentScenePath() {
        return SceneManager::GetInstance().GetCurrentScenePath();
    }
}

// ============================================================================
// WINDOW SYSTEM WRAPPERS
// ============================================================================
#include "WindowManager.hpp"

namespace WindowWrappers {
    inline int GetWindowWidth() {
        return WindowManager::GetWindowWidth();
    }

    inline int GetWindowHeight() {
        return WindowManager::GetWindowHeight();
    }

    inline int GetViewportWidth() {
        return WindowManager::GetViewportWidth();
    }

    inline int GetViewportHeight() {
        return WindowManager::GetViewportHeight();
    }

    inline bool IsWindowFocused() {
        return WindowManager::IsWindowFocused();
    }

    inline bool IsWindowMinimized() {
        return WindowManager::IsWindowMinimized();
    }

    inline void SetWindowTitle(const std::string& title) {
        WindowManager::SetWindowTitle(title.c_str());
    }

    inline void RequestClose() {
        WindowManager::SetWindowShouldClose();
    }

    inline void SetCursorLocked(bool locked) {
        WindowManager::SetCursorLocked(locked);
    }

    inline bool IsCursorLocked() {
        return WindowManager::IsCursorLocked();
    }
}

// ============================================================================
// DEBUG DRAW WRAPPERS
// ============================================================================
#include "Graphics/DebugDraw/DebugDrawSystem.hpp"

namespace DebugDrawWrappers {
    inline void DrawLine(float startX, float startY, float startZ,
                         float endX, float endY, float endZ,
                         float r, float g, float b, float duration) {
        DebugDrawSystem::DrawLine(
            Vector3D(startX, startY, startZ),
            Vector3D(endX, endY, endZ),
            Vector3D(r, g, b),
            duration
        );
    }
    
    inline void DrawCube(float x, float y, float z,
                         float scaleX, float scaleY, float scaleZ,
                         float r, float g, float b, float duration) {
        DebugDrawSystem::DrawCube(
            Vector3D(x, y, z),
            Vector3D(scaleX, scaleY, scaleZ),
            Vector3D(r, g, b),
            duration
        );
    }
    
    inline void DrawSphere(float x, float y, float z,
                           float radius,
                           float r, float g, float b, float duration) {
        DebugDrawSystem::DrawSphere(
            Vector3D(x, y, z),
            radius,
            Vector3D(r, g, b),
            duration
        );
    }
}

// ============================================================================
// AUDIO MANAGER WRAPPERS
// ============================================================================
#include "Sound/AudioManager.hpp"

namespace AudioManagerWrappers {
    inline void StopAll() {
        AudioManager::GetInstance().StopAll();
    }
    
    inline void SetMasterVolume(float volume) {
        AudioManager::GetInstance().SetMasterVolume(volume);
    }
    
    inline float GetMasterVolume() {
        return AudioManager::GetInstance().GetMasterVolume();
    }
    
    inline void SetGlobalPaused(bool paused) {
        AudioManager::GetInstance().SetGlobalPaused(paused);
    }
}

#include "Animation/AnimationComponent.hpp"
// ============================================================================
// ANIMATION SYSTEM WRAPPERS
// ============================================================================
namespace AnimationWrappers {
    inline void PlayClip(AnimationComponent& myAnim,std::size_t clipIndex, bool loop)
    {
        myAnim.PlayClip(clipIndex, loop);   //set the clip and loop flag
        myAnim.Play();                      //start the Playback
    }

    inline void PlayOnce(AnimationComponent& myAnim, std::size_t clipIndex)
    {
        myAnim.PlayOnce(clipIndex);
    }

    inline void Pause(AnimationComponent& myAnim)
    {
        myAnim.Pause();
    }

    inline void Stop(AnimationComponent& myAnim)
    {
        myAnim.Stop();
    }

    inline void SetSpeed(AnimationComponent& myAnim, float speed)
    {
        myAnim.SetSpeed(speed);
    }

    inline void SetLooping(AnimationComponent& myAnim, bool loop)
    {
        myAnim.SetLooping(loop);
    }

    inline bool IsPlaying(AnimationComponent& myAnim)
    {
        return myAnim.IsPlaying();
    }
}