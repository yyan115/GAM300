// ScriptSystem.hpp (excerpt)
#pragma once
#include "ECS/System.hpp"
#include "Script/ScriptComponentData.hpp" // engine POD
#include "ScriptComponent.h"    // the runtime type definition
#include "ScriptSerializer.h"   // if you call ScriptSerializer methods directly

#include "Script/ComponentProxy.h" // Exposing c++ functions to lua
#include <unordered_map>
#include <memory>
#include <mutex>

// forward
class ECSManager;
using Entity = unsigned int;


class ScriptSystem : public System {
public:
    ScriptSystem() = default;
    ~ScriptSystem(); // DECLARE only. definition goes in .cpp

    void Initialise(ECSManager& ecsManager);
    void Update();
    void Shutdown();
    void ReloadScriptForEntity(Entity e, ECSManager& ecsManager);
    bool CallEntityFunction(Entity e, const std::string& funcName, ECSManager& ecsManager);
    void ReloadSystem();
    void ReloadAllInstances();

    // Return LUA registry ref (instanceId) or LUA_NOREF if not available
    int GetInstanceRefForScript(Entity e, const std::string& scriptGuidStr);

    // Thread-safe call: calls a function on a specific instance (preferred)
    bool CallInstanceFunctionByScriptGuid(Entity e, const std::string& scriptGuidStr, const std::string& funcName);

    // An optional reload/invalidate callback (for caching clients) - TODO
    using InstancesChangedCb = std::function<void(Entity)>;
    void RegisterInstancesChangedCallback(InstancesChangedCb cb);
    void UnregisterInstancesChangedCallback(void* cbId);
private:
    // Notify registered callbacks that instances for entity 'e' changed.
    // Kept private: only ScriptSystem will call this when instances are created/destroyed/reloaded.
    void NotifyInstancesChanged(Entity e);

    bool EnsureInstanceForEntity(Entity e, ECSManager& ecsManager);
    void DestroyInstanceForEntity(Entity e);
    ScriptComponentData* GetScriptComponent(Entity e, ECSManager& ecsManager);
    const ScriptComponentData* GetScriptComponentConst(Entity e, const ECSManager& ecsManager) const;

    // Callback storage
    std::vector<std::pair<void*, InstancesChangedCb>> m_instancesChangedCbs;
    std::unordered_set<std::string> m_luaRegisteredComponents;
    std::unordered_map<Entity, std::vector<std::unique_ptr<Scripting::ScriptComponent>>> m_runtimeMap;
    ECSManager* m_ecs = nullptr;
    std::mutex m_mutex;

    bool m_needsReconcile = true;
};
