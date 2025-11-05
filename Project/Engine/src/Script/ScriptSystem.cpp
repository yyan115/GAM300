// ScriptSystem.cpp
#include "pch.h"
// ScriptSystem.cpp
#include "Script/ScriptSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "Script/ScriptComponentData.hpp"
#include "Logging.hpp"

// *Now* include scripting-project headers (must be visible to the engine project)
#include "ScriptComponent.h"    // the runtime type definition
#error "Included Scripting ScriptComponent.h — OK"
#include "ScriptSerializer.h"   // if you call ScriptSerializer methods directly
#include "Scripting.h"          // for public glue functions used

#include <fstream>
#include <sstream>
#include <algorithm>

// Define destructor where Scripting::ScriptComponent is a complete type
ScriptSystem::~ScriptSystem() = default; 
void ScriptSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;

    // Only set a disk fallback reader if nobody registered a FS callback earlier.
    static bool s_fsRegistered = false;
    if (!s_fsRegistered) {
        Scripting::SetFileSystemReadAllText([](const std::string& path, std::string& out) -> bool {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs.good()) return false;
            std::ostringstream ss;
            ss << ifs.rdbuf();
            out = ss.str();
            return true;
            });
        s_fsRegistered = true;
    }

    ENGINE_PRINT("[ScriptSystem] Initialised\n");
}

void ScriptSystem::Update(float dt, ECSManager& ecsManager) {
    // advance coroutines & runtime tick if runtime initialized
    if (Scripting::GetLuaState()) Scripting::Tick(dt);

    // iterate over entities matched to this system (System::entities)
    for (Entity e : entities) {
        ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
        if (!comp || !comp->enabled) continue;

        if (!EnsureInstanceForEntity(e, ecsManager)) continue;

        // call instance Update(dt) via runtime object's public API
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_runtimeMap.find(e);
            if (it != m_runtimeMap.end() && it->second) {
                it->second->Update(dt);
            }
        }
    }

    // cleanup runtime instances for entities that no longer belong to this system
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::vector<Entity> toRemove;
        toRemove.reserve(8);
        for (auto& p : m_runtimeMap) {
            Entity e = p.first;
            if (entities.find(e) == entities.end()) toRemove.push_back(e);
        }
        for (Entity e : toRemove) {
            DestroyInstanceForEntity(e);
        }
    }
}

void ScriptSystem::Shutdown() {
    std::lock_guard<std::mutex> lk(m_mutex);

    // best-effort: call OnDisable and release runtime objects
    for (auto& p : m_runtimeMap) {
        auto& ptr = p.second;
        if (ptr) {
            if (Scripting::GetLuaState()) ptr->OnDisable();
            // unique_ptr cleanup will call destructor which frees Lua refs if possible
        }
    }
    m_runtimeMap.clear();

    // clear engine POD runtime flags if any entities remain
    if (m_ecs) {
        for (Entity e : entities) {
            ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
            if (sc) { sc->instanceCreated = false; sc->instanceId = -1; }
        }
    }

    ENGINE_PRINT("[ScriptSystem] Shutdown complete\n");
}

bool ScriptSystem::EnsureInstanceForEntity(Entity e, ECSManager& ecsManager) {
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return false;

    // if runtime already created, update POD and return
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second) {
            comp->instanceId = it->second->GetInstanceRef();
            comp->instanceCreated = true;
            return true;
        }
    }

    if (!Scripting::GetLuaState()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] runtime missing; cannot create script for entity ", e, "\n");
        return false;
    }

    if (comp->scriptPath.empty()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] empty scriptPath for entity ", e, "\n");
        return false;
    }

    // create runtime object (scripting-project)
    auto runtimeComp = std::make_unique<Scripting::ScriptComponent>();
    if (!runtimeComp->AttachScript(comp->scriptPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] AttachScript failed for ", comp->scriptPath.c_str(), " entity=", e, "\n");
        return false;
    }

    // optional preserve keys registration via public glue if you use that API
    if (!comp->preserveKeys.empty()) {
        Scripting::RegisterInstancePreserveKeys(runtimeComp->GetInstanceRef(), comp->preserveKeys);
    }

    // cache runtime object and update POD
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_runtimeMap[e] = std::move(runtimeComp);
        comp->instanceId = m_runtimeMap[e]->GetInstanceRef();
        comp->instanceCreated = true;
    }

    // call lifecycle entry functions
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second) {
            it->second->Awake();
            it->second->Start();
        }
    }

    return true;
}

void ScriptSystem::DestroyInstanceForEntity(Entity e) {
    std::unique_ptr<Scripting::ScriptComponent> runtimePtr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it == m_runtimeMap.end()) return;
        runtimePtr = std::move(it->second);
        m_runtimeMap.erase(it);
    }

    if (runtimePtr) {
        if (Scripting::GetLuaState()) runtimePtr->OnDisable();
        // runtimePtr destructor runs automatically when it goes out of scope
    }

    if (m_ecs) {
        ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
        if (sc) { sc->instanceCreated = false; sc->instanceId = -1; }
    }
}

void ScriptSystem::ReloadScriptForEntity(Entity e, ECSManager& ecsManager) {
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return;

    if (!Scripting::GetLuaState()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Cannot reload: scripting runtime missing\n");
        return;
    }

    std::string preservedJson;

    // extract state then destroy old runtime
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second) {
            if (!comp->preserveKeys.empty()) preservedJson = it->second->SerializeState();
            if (Scripting::GetLuaState()) it->second->OnDisable();
            m_runtimeMap.erase(it);
        }
    }

    comp->instanceCreated = false;
    comp->instanceId = -1;

    // create new instance
    if (!EnsureInstanceForEntity(e, ecsManager)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Reload failed to create new instance for entity ", e, "\n");
        return;
    }

    // reinject preserved state
    if (!preservedJson.empty()) {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second) {
            it->second->DeserializeState(preservedJson);
        }
    }
}

bool ScriptSystem::CallEntityFunction(Entity e, const std::string& funcName, ECSManager& ecsManager) {
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return false;

    if (!comp->instanceCreated) {
        if (!EnsureInstanceForEntity(e, ecsManager)) return false;
    }

    int instRef = -1;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it == m_runtimeMap.end() || !it->second) return false;
        instRef = it->second->GetInstanceRef();
    }

    if (!Scripting::GetLuaState()) return false;
    // Use Scripting public API to call function by name on registry ref
    return Scripting::CallInstanceFunction(instRef, funcName);
}

ScriptComponentData* ScriptSystem::GetScriptComponent(Entity e, ECSManager& ecsManager) {
    try {
        if (!ecsManager.HasComponent<ScriptComponentData>(e)) return nullptr;
        return &ecsManager.GetComponent<ScriptComponentData>(e);
    }
    catch (...) {
        return nullptr;
    }
}

const ScriptComponentData* ScriptSystem::GetScriptComponentConst(Entity e, const ECSManager& ecsManager) const {
    // Delegate to the non-const implementation to avoid duplicating logic.
    // We must const_cast 'this' because GetScriptComponent is non-const;
    // this is safe here because GetScriptComponent does not mutate ScriptSystem state.
    ECSManager& nonConstEcs = const_cast<ECSManager&>(ecsManager);
    return const_cast<ScriptSystem*>(this)->GetScriptComponent(e, nonConstEcs);
}