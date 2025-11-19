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
    void Update(float dt, ECSManager& ecsManager);
    void Shutdown();
    void ReloadScriptForEntity(Entity e, ECSManager& ecsManager);
    bool CallEntityFunction(Entity e, const std::string& funcName, ECSManager& ecsManager);

private:
    bool EnsureInstanceForEntity(Entity e, ECSManager& ecsManager);
    void DestroyInstanceForEntity(Entity e);
    ScriptComponentData* GetScriptComponent(Entity e, ECSManager& ecsManager);
    const ScriptComponentData* GetScriptComponentConst(Entity e, const ECSManager& ecsManager) const;

    std::unordered_set<std::string> m_luaRegisteredComponents;
    std::unordered_map<Entity, std::unique_ptr<Scripting::ScriptComponent>> m_runtimeMap;
    ECSManager* m_ecs = nullptr;
    std::mutex m_mutex;
};
