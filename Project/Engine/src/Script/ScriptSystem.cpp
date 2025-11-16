// ScriptSystem.cpp
#include "pch.h"
// ScriptSystem.cpp
#include "Script/ScriptSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "Script/ScriptComponentData.hpp"

#include "Logging.hpp"

#include "Script/Scripting.h"          // for public glue functions used

#include <fstream>
#include <sstream>
#include <algorithm>


// Define destructor where Scripting::ScriptComponent is a complete type
ScriptSystem::~ScriptSystem() = default; 
void ScriptSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;

    // --- Build / register Transform TypeDescriptor inline (raw) ---
    // We keep the created descriptors alive for program lifetime by storing them
    // in a static vector (intended leak so reflection stays valid).
    static bool s_transformDescRegistered = false;
    static std::vector<TypeDescriptor*> s_leakedDescriptors;
    if (!s_transformDescRegistered) {
        // Try to get an existing descriptor first.
        TypeDescriptor* transformTd = nullptr;
        try { transformTd = TypeResolver<Transform>::Get(); }
        catch (...) { transformTd = nullptr; }

        // Register the component with an explicit TypeDescriptor (raw getter + td)
        ComponentRegistry::Instance().RegisterRaw(
            "Transform",
            [](ECSManager* ecs, Entity e) -> void* {
                if (!ecs->HasComponent<Transform>(e)) return nullptr;
                return &ecs->GetComponent<Transform>(e);
            },
            transformTd
        );

        s_transformDescRegistered = true;
    }

    // ensure metatable registered
    RegisterComponentProxyMeta(Scripting::GetLuaState());

    // install host get-component handler that uses ComponentRegistry
    Scripting::SetHostGetComponentHandler([this](lua_State* L, uint32_t entityId, const std::string& compName) -> bool {
        ENGINE_PRINT("[ScriptSystem] HostGetComponentHandler asked for comp=", compName, " entity=", entityId);

        if (!ComponentRegistry::Instance().Has(compName)) {
            lua_pushnil(L);
            return true;
        }

        auto getter = ComponentRegistry::Instance().GetGetter(compName);
        if (!getter) { lua_pushnil(L); return true; }
        void* compPtr = getter(m_ecs, static_cast<Entity>(entityId));
        if (!compPtr) { lua_pushnil(L); return true; }

        // push proxy userdata that will look up the component on access
        PushComponentProxy(L, m_ecs, static_cast<Entity>(entityId), compName);
        return true;
        });

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

    // If runtime already created, update POD and return
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second) {
            comp->instanceId = it->second->GetInstanceRef();
            comp->instanceCreated = true;
            return true;
        }
    }

    // Ensure Lua runtime exists and we are allowed to create instances now.
    if (!Scripting::GetLuaState()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] runtime missing; cannot create script for entity ", e, "\n");
        return false;
    }

    if (comp->scriptPath.empty()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] empty scriptPath for entity ", e, "\n");
        return false;
    }

    // Create runtime object (scripting-project)
    auto runtimeComp = std::make_unique<Scripting::ScriptComponent>();
    if (!runtimeComp->AttachScript(comp->scriptPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] AttachScript failed for ", comp->scriptPath.c_str(), " entity=", e, "\n");
        return false;
    }

    // Register preserve keys (Scripting glue expected to accept the registry ref returned by ScriptComponent::GetInstanceRef())
    if (!comp->preserveKeys.empty()) {
        Scripting::RegisterInstancePreserveKeys(runtimeComp->GetInstanceRef(), comp->preserveKeys);
    }

    // Move runtime into our map and update POD. Do Lua-related per-instance operations *while holding the mutex*
    // so we avoid races with other threads trying to query m_runtimeMap.
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_runtimeMap[e] = std::move(runtimeComp);
        Scripting::ScriptComponent* scPtr = m_runtimeMap[e].get();

        // update engine POD (debug mirror)
        comp->instanceId = scPtr->GetInstanceRef();
        comp->instanceCreated = true;

        // If this entity had pending serialized Lua state from scene load, try to apply it now.
        if (!comp->pendingInstanceState.empty()) {
            try {
                bool ok = scPtr->DeserializeState(comp->pendingInstanceState);
                if (ok) {
                    // applied successfully, clear pending state
                    comp->pendingInstanceState.clear();
                }
                else {
                    ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Failed to deserialize pending script state for entity ", e, "\n");
                    // keep pending; ScriptSystem may retry later or log for debugging
                }
            }
            catch (const std::exception& ex) {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Exception while applying pending script state for entity ", e, " : ", ex.what(), "\n");
            }
            catch (...) {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Unknown exception while applying pending script state for entity ", e, "\n");
            }
        }

        // Bind the Lua instance to this entity so scripts can call self:GetComponent(...) or host GetComponent handler can route.
        // NOTE: There are two common approaches — pick the one your Scripting API expects:
        // 1) If you have a Scripting::BindInstanceToEntity(instanceRefOrId, entityId) glue function, use it.
        //    (Leave the call below as-is if your glue accepts the value returned by ScriptComponent::GetInstanceRef()).
        // 2) If you DON'T have that glue, we set an 'entityId' field on the instance table in Lua so scripts can read it (fallback).
        //
        // The code below first tries the public glue; if it is not available or returns false, it falls back to setting instance.entityId.
        {
            int instRef = scPtr->GetInstanceRef();
            bool bound = false;
            // Attempt public glue binding (if implemented). If your Scripting API expects a different integer
            // (e.g. a numeric "instance id" produced by Scripting::CreateInstanceFromFile), ensure the glue
            // accepts the registry ref value or adapt accordingly.
            try {
                // NOTE: If Scripting::BindInstanceToEntity is not implemented in your Scripting.h, you'll get a link error.
                // Replace this call with your actual binding API, or remove block and use the fallback below.
                bound = Scripting::BindInstanceToEntity(instRef, static_cast<uint32_t>(e));
            }
            catch (...) {
                bound = false;
            }

            if (!bound) {
                // Fallback: attach a plain numeric field "entityId" to the instance table so scripts can access it.
                // This avoids depending on additional glue APIs. We need to manipulate Lua stack; do it using ScriptComponent's API
                // through the Scripting public surface if possible. If not available, we do a minimal direct Lua ops below.
                lua_State* L = ::Scripting::GetLuaState();
                if (L) {
                    // push instance table from registry and set field entityId = <e>
                    lua_rawgeti(L, LUA_REGISTRYINDEX, instRef);   // push instance table
                    if (lua_istable(L, -1)) {
                        lua_pushinteger(L, static_cast<lua_Integer>(e));
                        lua_setfield(L, -2, "entityId"); // instance.entityId = e
                    }
                    lua_pop(L, 1); // pop instance (or nil)
                }
            }
        }
    } // release lock

    // Call lifecycle entry functions outside of the larger mutex-held block above (we still need to protect map access)
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