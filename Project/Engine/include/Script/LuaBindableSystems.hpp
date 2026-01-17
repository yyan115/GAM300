#pragma once
#include "Math/Vector3D.hpp"
#include "Reflection/ReflectionBase.hpp"
#include <tuple>

// ============================================================================
// VECTOR2D (for 2D values like axis input, pointer position)
// ============================================================================
struct Vector2D {
    REFL_SERIALIZABLE

    float x, y;
    Vector2D() : x(0), y(0) {}
    Vector2D(float _x, float _y) : x(_x), y(_y) {}
};

// ============================================================================
// INPUT SYSTEM WRAPPERS
// ============================================================================
#include "Input/Keys.h"
#include "Input/InputManager.h"
#include <unordered_map>
#include <string>

namespace InputWrappers {
    // Action-based input (platform-agnostic)
    inline bool IsActionPressed(const std::string& action) {
        if (!g_inputManager) {
            std::cout << "[LuaWrapper] IsActionPressed: g_inputManager is NULL!" << std::endl;
            return false;
        }
        bool result = g_inputManager->IsActionPressed(action);
        if (result) {
            std::cout << "[LuaWrapper] IsActionPressed('" << action << "') = true" << std::endl;
        }
        return result;
    }

    inline bool IsActionJustPressed(const std::string& action) {
        if (!g_inputManager) {
            std::cout << "[LuaWrapper] IsActionJustPressed: g_inputManager is NULL!" << std::endl;
            return false;
        }
        bool result = g_inputManager->IsActionJustPressed(action);
        if (result) {
            std::cout << "[LuaWrapper] IsActionJustPressed('" << action << "') = true" << std::endl;
        }
        return result;
    }

    inline bool IsActionJustReleased(const std::string& action) {
        if (!g_inputManager) return false;
        return g_inputManager->IsActionJustReleased(action);
    }

    // Axis input returns Vector2D (access with .x and .y in Lua)
    inline Vector2D GetAxis(const std::string& axisName) {
        if (!g_inputManager) return Vector2D(0.0f, 0.0f);
        glm::vec2 axis = g_inputManager->GetAxis(axisName);
        return Vector2D(axis.x, axis.y);
    }

    // Batch API for Lua optimization - returns all action states at once
    // Lua example: local states = UnifiedInput.GetAllActionStates()
    //              if states["Jump"] then ... end
    inline std::unordered_map<std::string, bool> GetAllActionStates() {
        if (!g_inputManager) return {};
        return g_inputManager->GetAllActionStates();
    }

    // Get all axis states at once
    // Returns map of axis name -> Vector2D with x, y properties
    inline std::unordered_map<std::string, Vector2D> GetAllAxisStates() {
        if (!g_inputManager) return {};

        auto axisMap = g_inputManager->GetAllAxisStates();
        std::unordered_map<std::string, Vector2D> result;

        for (const auto& [name, vec] : axisMap) {
            result[name] = Vector2D(vec.x, vec.y);
        }

        return result;
    }

    // Pointer abstraction (for UI)
    inline bool IsPointerPressed() {
        if (!g_inputManager) return false;
        return g_inputManager->IsPointerPressed();
    }

    inline bool IsPointerJustPressed() {
        if (!g_inputManager) return false;
        return g_inputManager->IsPointerJustPressed();
    }

    inline Vector2D GetPointerPosition() {
        if (!g_inputManager) return Vector2D(0.0f, 0.0f);
        glm::vec2 pos = g_inputManager->GetPointerPosition();
        return Vector2D(pos.x, pos.y);
    }

    // Multi-touch support
    inline int GetTouchCount() {
        if (!g_inputManager) return 0;
        return g_inputManager->GetTouchCount();
    }

    inline Vector2D GetTouchPosition(int index) {
        if (!g_inputManager) return Vector2D(0.0f, 0.0f);
        glm::vec2 pos = g_inputManager->GetTouchPosition(index);
        return Vector2D(pos.x, pos.y);
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


//// ============================================================================
//// RIGIDBODY SYSTEM WRAPPERS
//// ============================================================================
//#include "Physics/RigidBodyComponent.hpp"
//
//namespace RigidBodySystemWrappers {
//    inline void AddForce(RigidBodyComponent& rigidbody, float x, float y, float z)
//    {
//        rigidbody.AddForce(Vector3D(x,y,z));
//    }
//    inline void AddTorque(RigidBodyComponent& rigidbody, float x, float y, float z)
//    {
//        rigidbody.AddTorque(Vector3D(x, y, z));
//    }
//    inline void AddImpulse(RigidBodyComponent& rigidbody, float x, float y, float z)
//    {
//        rigidbody.AddImpulse(Vector3D(x, y, z));
//    }
//}



// ============================================================================
// CHARACTER CONTROLLER WRAPPERS
// ============================================================================
#include "Physics/Kinematics/CharacterController.hpp"       //to be removed 
#include "Physics/Kinematics/CharacterControllerSystem.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "ECS/ECSManager.hpp"

using Entity = unsigned int;

namespace CharacterControllerWrappers {

    inline CharacterController* CreateController(Entity id,
        ColliderComponent* collider,
        Transform* transform)
    {
        JPH::PhysicsSystem* physicsSystem = PhysicsSystemWrappers::GetSystem();

        if (!physicsSystem) {
            std::cerr << "[ERROR] Cannot create CharacterController - PhysicsSystem unavailable!" << std::endl;
            return nullptr;
        }

        if (!collider || !transform) {
            std::cerr << "[ERROR] Cannot create CharacterController - invalid inputs!" << std::endl;
            return nullptr;
        }

        auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        if (!ecsManager.characterControllerSystem)
        {
            std::cerr << "[ERROR] Cannot create CharacterController - CharacterControllerSystem unavailable!" << std::endl;
            return nullptr;
        }
        std::cout << "UPDATED 0" << std::endl;
        //call system to create controller
        return ecsManager.characterControllerSystem->CreateController(id, *collider, *transform);

        //CharacterController* controller = new CharacterController(physicsSystem);
        //return controller->CreateController(*collider, *transform);
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
