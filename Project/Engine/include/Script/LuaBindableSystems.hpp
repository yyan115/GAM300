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
// TOUCH INFO (for full touch tracking - phases, IDs, etc.)
// ============================================================================
struct TouchInfo {
    int id = -1;                    // Unique finger ID (persists while finger is down)
    std::string phase = "none";     // "began", "moved", "stationary", "ended"
    Vector2D position;              // Current position (normalized 0-1)
    Vector2D startPosition;         // Where the touch started
    Vector2D delta;                 // Movement since last frame
    std::string entity = "";        // Entity name if touch is on UI, empty string if none
    float duration = 0.0f;          // How long the touch has been active (seconds)
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
        if (!g_inputManager) return false;
        return g_inputManager->IsActionPressed(action);
    }

    inline bool IsActionJustPressed(const std::string& action) {
        if (!g_inputManager) return false;
        return g_inputManager->IsActionJustPressed(action);
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

    // Entity-based touch position (for joysticks - Android)
    // Returns touch position relative to entity center in game units
    inline Vector2D GetActionTouchPosition(const std::string& action) {
        if (!g_inputManager) return Vector2D(0.0f, 0.0f);
        glm::vec2 pos = g_inputManager->GetActionTouchPosition(action);
        return Vector2D(pos.x, pos.y);
    }

    // Camera drag support (Android - unhandled touches)
    inline bool IsDragging() {
        if (!g_inputManager) return false;
        return g_inputManager->IsDragging();
    }

    inline Vector2D GetDragDelta() {
        if (!g_inputManager) return Vector2D(0.0f, 0.0f);
        glm::vec2 delta = g_inputManager->GetDragDelta();
        return Vector2D(delta.x, delta.y);
    }

    // ========== Full Touch System ==========

    // Convert phase enum to string for Lua
    inline std::string PhaseToString(InputManager::TouchPhase phase) {
        switch (phase) {
            case InputManager::TouchPhase::Began: return "began";
            case InputManager::TouchPhase::Moved: return "moved";
            case InputManager::TouchPhase::Stationary: return "stationary";
            case InputManager::TouchPhase::Ended: return "ended";
            default: return "none";
        }
    }

    // Get all touches as a vector of TouchInfo
    inline std::vector<TouchInfo> GetTouches() {
        std::vector<TouchInfo> result;
        if (!g_inputManager) return result;

        auto touches = g_inputManager->GetTouches();
        for (const auto& t : touches) {
            TouchInfo info;
            info.id = t.id;
            info.phase = PhaseToString(t.phase);
            info.position = Vector2D(t.position.x, t.position.y);
            info.startPosition = Vector2D(t.startPosition.x, t.startPosition.y);
            info.delta = Vector2D(t.delta.x, t.delta.y);
            info.entity = t.entityName;
            info.duration = t.duration;
            result.push_back(info);
        }
        return result;
    }

    // Get a specific touch by ID
    inline TouchInfo GetTouchById(int touchId) {
        TouchInfo info;
        if (!g_inputManager) return info;

        auto t = g_inputManager->GetTouchById(touchId);
        info.id = t.id;
        info.phase = PhaseToString(t.phase);
        info.position = Vector2D(t.position.x, t.position.y);
        info.startPosition = Vector2D(t.startPosition.x, t.startPosition.y);
        info.delta = Vector2D(t.delta.x, t.delta.y);
        info.entity = t.entityName;
        info.duration = t.duration;
        return info;
    }
}

// ============================================================================
// PHYSICS SYSTEM WRAPPERS
// ============================================================================
#include "Physics/PhysicsSystem.hpp"
#include "ECS/ECSRegistry.hpp"

namespace PhysicsSystemWrappers {
    inline PhysicsSystem* g_PhysicsSystem = nullptr;

    // Cache for overlap results
    inline std::unordered_map<int, std::vector<Entity>> g_OverlapCache;
    inline int g_NextCacheID = 1;

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

    // Get overlapping entities and return a cache ID
    // Usage: local cacheId = Physics.GetOverlappingEntities(entityId)
    inline int Lua_GetOverlappingEntities(Entity entity) {
        if (!g_PhysicsSystem) return 0;

        std::vector<Entity> overlapping;
        bool success = g_PhysicsSystem->GetOverlappingEntities(entity, overlapping);

        if (!success) return 0;

        // Store in cache
        int cacheId = g_NextCacheID++;
        g_OverlapCache[cacheId] = std::move(overlapping);

        return cacheId;
    }

    // Get count from cache
    inline int GetOverlapCount(int cacheId) {
        auto it = g_OverlapCache.find(cacheId);
        if (it == g_OverlapCache.end()) return 0;
        return static_cast<int>(it->second.size());
    }

    // Get entity at index from cache
    inline Entity GetOverlapAt(int cacheId, int index) {
        auto it = g_OverlapCache.find(cacheId);
        if (it == g_OverlapCache.end()) return 0;
        if (index < 0 || index >= it->second.size()) return 0;
        return it->second[index];
    }

    // Clear cache entry (call when done)
    inline void ClearOverlapCache(int cacheId) {
        g_OverlapCache.erase(cacheId);
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

    inline void DestroyByEntity(Entity id)
    {
        auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecsManager.characterControllerSystem)
            ecsManager.characterControllerSystem->RemoveController(id);
    }

    inline void UpdateAll(float dt)
    {
        auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecsManager.characterControllerSystem)
            ecsManager.characterControllerSystem->Update(dt, ecsManager);
    }

    inline void ClearAll()
    {
        auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecsManager.characterControllerSystem)
            ecsManager.characterControllerSystem->Shutdown();
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

    //PAUSE FUNCTIONS
    inline void SetPaused(bool paused) { return TimeManager::SetPaused(paused); }
    inline bool IsPaused() { return TimeManager::IsPaused(); }

}

// ============================================================================
// SCENE SYSTEM WRAPPERS
// ============================================================================
#include "Scene/SceneManager.hpp"

namespace SceneWrappers {
    inline void LoadScene(const std::string& scenePath) {
        // Pass true for callingFromLua so editor stays in play mode during scene transitions
        SceneManager::GetInstance().LoadScene(scenePath, true);
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

    // Bus/AudioMixerGroup controls (for BGM, SFX, Master buses)
    inline void SetBusVolume(const std::string& busName, float volume) {
        AudioManager::GetInstance().SetBusVolume(busName, volume);
    }

    inline float GetBusVolume(const std::string& busName) {
        return AudioManager::GetInstance().GetBusVolume(busName);
    }

    inline void SetBusPaused(const std::string& busName, bool paused) {
        AudioManager::GetInstance().SetBusPaused(busName, paused);
    }
}

// ============================================================================
// PLATFORM WRAPPERS
// ============================================================================

namespace PlatformWrappers {
    // Returns true if running on Android, false on desktop
    inline bool IsAndroid() {
#ifdef ANDROID
        return true;
#else
        return false;
#endif
    }

    // Returns true if running on desktop (Windows/Linux/Mac)
    inline bool IsDesktop() {
#ifdef ANDROID
        return false;
#else
        return true;
#endif
    }

    // Returns platform name as string
    inline std::string GetPlatformName() {
#ifdef ANDROID
        return "Android";
#elif defined(_WIN32)
        return "Windows";
#elif defined(__linux__)
        return "Linux";
#elif defined(__APPLE__)
        return "macOS";
#else
        return "Unknown";
#endif
    }
}

// ============================================================================
// GAME SETTINGS WRAPPERS
// ============================================================================
#include "Settings/GameSettings.hpp"

namespace GameSettingsWrappers {
    // Initialization (safe to call multiple times)
    inline void Init() {
        GameSettingsManager::GetInstance().Initialize();
    }

    // Reset all settings to defaults
    inline void ResetToDefaults() {
        GameSettingsManager::GetInstance().ResetToDefaults();
    }

    // Save settings to disk (call when closing settings menu)
    inline void Save() {
        GameSettingsManager::GetInstance().SaveSettings();
    }

    // Save only if settings changed (optimization)
    inline void SaveIfDirty() {
        GameSettingsManager::GetInstance().SaveIfDirty();
    }

    // Audio setters (mark dirty, don't auto-save for performance)
    inline void SetMasterVolume(float volume) {
        GameSettingsManager::GetInstance().SetMasterVolume(volume);
    }

    inline void SetBGMVolume(float volume) {
        GameSettingsManager::GetInstance().SetBGMVolume(volume);
    }

    inline void SetSFXVolume(float volume) {
        GameSettingsManager::GetInstance().SetSFXVolume(volume);
    }

    // Audio getters
    inline float GetMasterVolume() {
        return GameSettingsManager::GetInstance().GetMasterVolume();
    }

    inline float GetBGMVolume() {
        return GameSettingsManager::GetInstance().GetBGMVolume();
    }

    inline float GetSFXVolume() {
        return GameSettingsManager::GetInstance().GetSFXVolume();
    }

    // Graphics setters (mark dirty, don't auto-save for performance)
    inline void SetGamma(float gamma) {
        GameSettingsManager::GetInstance().SetGamma(gamma);
    }

    inline void SetExposure(float exposure) {
        GameSettingsManager::GetInstance().SetExposure(exposure);
    }

    // Graphics getters
    inline float GetGamma() {
        return GameSettingsManager::GetInstance().GetGamma();
    }

    inline float GetExposure() {
        return GameSettingsManager::GetInstance().GetExposure();
    }

    // Default value getters (for UI reset functionality)
    inline float GetDefaultMasterVolume() {
        return GameSettingsManager::GetDefaultMasterVolume();
    }

    inline float GetDefaultBGMVolume() {
        return GameSettingsManager::GetDefaultBGMVolume();
    }

    inline float GetDefaultSFXVolume() {
        return GameSettingsManager::GetDefaultSFXVolume();
    }

    inline float GetDefaultGamma() {
        return GameSettingsManager::GetDefaultGamma();
    }

    inline float GetDefaultExposure() {
        return GameSettingsManager::GetDefaultExposure();
    }
}

// ============================================================================
// NAV SYSTEM WRAPPERS
// ============================================================================
#include "Game AI/NavSystem.hpp"

namespace NavWrappers {

    inline int RequestPathXZ(lua_State* L)
    {
        // Expect 5 numbers on the stack
        float sx = (float)luaL_checknumber(L, 1);
        float sz = (float)luaL_checknumber(L, 2);
        float gx = (float)luaL_checknumber(L, 3);
        float gz = (float)luaL_checknumber(L, 4);
        Entity e = (Entity)luaL_checknumber(L, 5);

        const auto path = NavSystem::Get().RequestPathXZ(sx, sz, gx, gz, e);

        lua_newtable(L); // push result table

        int i = 1;
        for (const auto& p : path)
        {
            lua_newtable(L);               // point table
            lua_pushnumber(L, p.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, p.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, p.z); lua_setfield(L, -2, "z");

            lua_rawseti(L, -2, i++);       // result[i] = point
        }

        return 1; // returning 1 value (the table)
    }

    inline float GetGroundY(Entity entity) {
        return NavSystem::Get().GetGroundY(entity);
    }
}



// ============================================================================
// 
// ============================================================================

#include "Script/ScriptComponentData.hpp"
#include "ECS/NameComponent.hpp"

namespace EntityQueryWrappers {

    // ============================================================================
    // CACHE MANAGEMENT
    // ============================================================================

    struct ScriptQueryCache {
        std::vector<Entity> entities;
        std::string scriptFilename;
        float timeSinceUpdate;
        float updateInterval;

        ScriptQueryCache()
            : scriptFilename("")
            , timeSinceUpdate(999.0f)
            , updateInterval(1.0f)
        {
        }
    };

    static std::unordered_map<std::string, ScriptQueryCache> s_scriptQueryCache;

    inline std::string GetFilenameWithoutExtension(const std::string& path) {
        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos)
            ? path.substr(lastSlash + 1)
            : path;

        size_t lastDot = filename.find_last_of('.');
        if (lastDot != std::string::npos) {
            filename = filename.substr(0, lastDot);
        }

        return filename;
    }

    inline void UpdateCacheForScript(const std::string& scriptName) {
        std::string targetFilename = GetFilenameWithoutExtension(scriptName);

        //printf("\n========== C++ UpdateCacheForScript ==========\n");
        //printf("[C++ DEBUG] Input scriptName: '%s'\n", scriptName.c_str());
        //printf("[C++ DEBUG] Target filename (extracted): '%s'\n", targetFilename.c_str());

        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        std::vector<Entity> allEntities = ecsManager.GetAllEntities();

        //printf("[C++ DEBUG] Total entities in scene: %zu\n", allEntities.size());

        std::vector<Entity> results;
        int entitiesWithScript = 0;
        int entitiesWithScriptAndTransform = 0;

        for (Entity entity : allEntities) {
            auto scriptCompOpt = ecsManager.TryGetComponent<ScriptComponentData>(entity);
            if (!scriptCompOpt.has_value()) continue;

            ScriptComponentData& scriptComp = scriptCompOpt.value().get();

            for (const auto& script : scriptComp.scripts) {
                if (!script.enabled) continue;

                std::string scriptFilename = GetFilenameWithoutExtension(script.scriptPath);

                if (scriptFilename == targetFilename) {
                    entitiesWithScript++;
                    //printf("[C++ DEBUG] Entity %u has script: '%s' (full path: '%s')\n",
                    //    entity, scriptFilename.c_str(), script.scriptPath.c_str());
                    //printf("[C++ DEBUG]   *** MATCH! Script filename matches target ***\n");

                    // Check for Transform
                    auto transformOpt = ecsManager.TryGetComponent<Transform>(entity);
                    if (transformOpt.has_value()) {
                        entitiesWithScriptAndTransform++;
                        auto& transform = transformOpt.value().get();
                        Vector3D pos = transform.worldPosition;

                        //printf("[C++ DEBUG]  Entity has Transform at position: (%.2f, %.2f, %.2f)\n",
                        //    pos.x, pos.y, pos.z);

                        results.push_back(entity);
                    }
                    else {
                        printf("[C++ DEBUG]  Entity has NO Transform - SKIPPING\n");
                    }
                    break;
                }
            }
        }

        auto& cache = s_scriptQueryCache[scriptName];
        cache.entities = results;
        cache.scriptFilename = targetFilename;
        cache.timeSinceUpdate = 0.0f;

        //printf("[C++ DEBUG] ========== CACHE UPDATE SUMMARY ==========\n");
        //printf("[C++ DEBUG] Entities with matching script: %d\n", entitiesWithScript);
        //printf("[C++ DEBUG] Entities with script AND Transform: %d\n", entitiesWithScriptAndTransform);
        //printf("[C++ DEBUG] Cached entities: %zu\n", results.size());

        //if (results.size() > 0) {
        //    printf("[C++ DEBUG] Cached entity IDs: ");
        //    for (size_t i = 0; i < results.size(); ++i) {
        //        printf("%u", results[i]);
        //        if (i < results.size() - 1) printf(", ");
        //    }
        //    printf("\n");
        //}
        //printf("========== C++ UpdateCacheForScript END ==========\n\n");
    }

    // ============================================================================
    // PUBLIC API
    // ============================================================================

    // Find entities with script - returns Lua table
    inline int FindEntitiesWithScript(lua_State* L) {
        //printf("\n========== C++ FindEntitiesWithScript CALLED ==========\n");

        const char* scriptPath = luaL_checkstring(L, 1);
        if (!scriptPath) {
            printf("[C++ DEBUG] ERROR: luaL_checkstring returned NULL!\n");
            lua_newtable(L);
            return 1;
        }

        std::string scriptName(scriptPath);
        //printf("[C++ DEBUG] Received from Lua: '%s'\n", scriptName.c_str());
        //printf("[C++ DEBUG] String length: %zu\n", scriptName.length());

        // Check cache
        auto it = s_scriptQueryCache.find(scriptName);
        if (it != s_scriptQueryCache.end()) {
            ScriptQueryCache& cache = it->second;

            //printf("[C++ DEBUG] Cache exists for '%s'\n", scriptName.c_str());
            //printf("[C++ DEBUG] Cache age: %.2f seconds (interval: %.2f)\n",
            //    cache.timeSinceUpdate, cache.updateInterval);

            if (cache.timeSinceUpdate < cache.updateInterval) {
                //printf("[C++ DEBUG] Cache still valid - using cached results\n");
                //printf("[C++ DEBUG] Cached entity count: %zu\n", cache.entities.size());

                // Build Lua table from cached results
                lua_newtable(L);
                int index = 1;
                for (Entity entity : cache.entities) {
                    //printf("[C++ DEBUG]   Pushing entity %u to Lua table at index %d\n", entity, index);
                    lua_pushinteger(L, static_cast<lua_Integer>(entity));
                    lua_rawseti(L, -2, index++);
                }

                //printf("[C++ DEBUG] Returning table with %d entries\n", index - 1);
                //printf("========== C++ FindEntitiesWithScript END (cached) ==========\n\n");
                return 1;
            }
            else {
                printf("[C++ DEBUG] Cache expired - updating\n");
            }
        }
        else {
            printf("[C++ DEBUG] No cache exists - creating new cache\n");
        }

        // Cache expired or doesn't exist - update it
        UpdateCacheForScript(scriptName);

        // Build Lua table from updated cache
        lua_newtable(L);
        int index = 1;

        //printf("[C++ DEBUG] Building Lua table from updated cache:\n");
        for (Entity entity : s_scriptQueryCache[scriptName].entities) {
            //printf("[C++ DEBUG]   Pushing entity %u to Lua table at index %d\n", entity, index);
            lua_pushinteger(L, static_cast<lua_Integer>(entity));
            lua_rawseti(L, -2, index++);
        }

        //printf("[C++ DEBUG] Returning table with %d entries\n", index - 1);
        //printf("========== C++ FindEntitiesWithScript END (updated) ==========\n\n");

        return 1;
    }

    // Update timing for all caches
    inline void UpdateCacheTiming(float deltaTime) {
        // Only log every 60 calls (~1 second at 60fps) to avoid spam
        static int callCount = 0;
        callCount++;

        //if (callCount % 60 == 0) {
        //    printf("[C++ DEBUG] UpdateCacheTiming called (deltaTime: %.4f)\n", deltaTime);
        //    printf("[C++ DEBUG] Current cache count: %zu\n", s_scriptQueryCache.size());

        //    for (auto& pair : s_scriptQueryCache) {
        //        printf("[C++ DEBUG]   Cache '%s': age=%.2fs, entities=%zu\n",
        //            pair.first.c_str(), pair.second.timeSinceUpdate, pair.second.entities.size());
        //    }
        //}

        for (auto& pair : s_scriptQueryCache) {
            pair.second.timeSinceUpdate += deltaTime;
        }
    }

    // Force update cache
    inline void UpdateEnemyCache(const std::string& scriptName) {
        //printf("[C++ DEBUG] UpdateEnemyCache called for: '%s'\n", scriptName.c_str());
        UpdateCacheForScript(scriptName);
    }

    // Set cache interval
    inline void SetCacheUpdateInterval(const std::string& scriptName, float intervalSeconds) {
        auto it = s_scriptQueryCache.find(scriptName);
        if (it != s_scriptQueryCache.end()) {
            it->second.updateInterval = intervalSeconds;
            //printf("[C++ DEBUG] Cache interval for '%s' set to %.2f seconds\n",
            //    scriptName.c_str(), intervalSeconds);
        }
        else {
            s_scriptQueryCache[scriptName].updateInterval = intervalSeconds;
            //printf("[C++ DEBUG] Created cache entry for '%s' with interval %.2f seconds\n",
            //    scriptName.c_str(), intervalSeconds);
        }
    }

    // Clear all caches
    inline void ClearEnemyCaches() {
        //printf("[C++ DEBUG] ClearEnemyCaches called - clearing %zu caches\n", s_scriptQueryCache.size());
        s_scriptQueryCache.clear();
    }

    // Get cache info
    inline std::tuple<int, float, float> GetCacheInfo(const std::string& scriptName) {
        auto it = s_scriptQueryCache.find(scriptName);
        if (it != s_scriptQueryCache.end()) {
            const ScriptQueryCache& cache = it->second;
            //printf("[C++ DEBUG] GetCacheInfo('%s'): count=%zu, age=%.2f, interval=%.2f\n",
            //    scriptName.c_str(), cache.entities.size(), cache.timeSinceUpdate, cache.updateInterval);
            return std::make_tuple(
                static_cast<int>(cache.entities.size()),
                cache.timeSinceUpdate,
                cache.updateInterval
            );
        }
        //printf("[C++ DEBUG] GetCacheInfo('%s'): No cache found\n", scriptName.c_str());
        return std::make_tuple(0, -1.0f, -1.0f);
    }

    // ============================================================================
    // FIXED: Get world position - returns 3 separate values (WITH DEBUG)
    // ============================================================================
    inline int GetEntityPosition(lua_State* L) {
        Entity entity = static_cast<Entity>(luaL_checkinteger(L, 1));

        //printf("[C++ DEBUG] GetEntityPosition called for entity %u\n", entity);

        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        auto transformOpt = ecsManager.TryGetComponent<Transform>(entity);
        if (!transformOpt.has_value()) {
            //printf("[C++ DEBUG]   Entity %u has NO Transform - returning (0, 0, 0)\n", entity);

            // Return (0, 0, 0) when no transform
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            return 3;  // Return 3 values
        }

        auto& transform = transformOpt.value().get();
        Vector3D worldPos = transform.worldPosition;

        //printf("[C++ DEBUG]   Entity %u position: (%.2f, %.2f, %.2f)\n",
        //    entity, worldPos.x, worldPos.y, worldPos.z);
        //printf("[C++ DEBUG]   Pushing 3 SEPARATE numbers to Lua stack\n");

        // Push 3 separate numbers (NOT a table!)
        lua_pushnumber(L, worldPos.x);
        lua_pushnumber(L, worldPos.y);
        lua_pushnumber(L, worldPos.z);

        //printf("[C++ DEBUG]   Returning 3 values\n");
        return 3;  // Return 3 values
    }

    // Get entity name - returns string
    inline std::string GetEntityName(Entity entity) {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        auto nameOpt = ecsManager.TryGetComponent<NameComponent>(entity);
        if (!nameOpt.has_value()) {
            return "";
        }

        return nameOpt.value().get().name;
    }

    // Check if entity is active - returns bool
    inline bool IsEntityActive(Entity entity) {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        return ecsManager.IsEntityActiveInHierarchy(entity);
    }

}