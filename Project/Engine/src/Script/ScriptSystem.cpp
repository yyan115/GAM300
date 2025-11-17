// ScriptSystem.cpp
#include "pch.h"
#include "Script/ScriptSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "Script/ScriptComponentData.hpp"
#include "Script/LuaBindableComponents.hpp"
#include <lua.hpp>
#include <LuaBridge.h>
#include "Logging.hpp"

#include "Scripting.h"          // for public glue functions used
#include "ECS/NameComponent.hpp"    // or wherever NameComponent is defined
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <set>


// Define destructor where Scripting::ScriptComponent is a complete type
ScriptSystem::~ScriptSystem() = default;

static std::unordered_map<std::string, std::function<void(lua_State*, void*)>> g_componentPushers;
static std::set<std::string> g_luaRegisteredComponents_global;
static bool g_luaBindingsDone = false;

// Template to register component getters into the existing ComponentRegistry.
// This is idempotent per component type.
template<typename CompT>
static void RegisterCompGetter(const char* compName) {
    static std::unordered_map<std::string, bool> s_registered;
    if (s_registered[compName]) return;
    ComponentRegistry::Instance().Register<CompT>(compName,
        [](ECSManager* ecs, Entity e) -> CompT* {
            if (!ecs) return nullptr;
            if (!ecs->HasComponent<CompT>(e)) return nullptr;
            return &ecs->GetComponent<CompT>(e);
        });
    s_registered[compName] = true;
}

// Template to register a pusher for a component type that will push a typed pointer via LuaBridge.
template<typename CompT>
static void RegisterCompPusher(const char* compName) {
    static std::unordered_map<std::string, bool> s_registered;
    if (s_registered[compName]) return;

    g_componentPushers[compName] = [](lua_State* L, void* ptr) {
        CompT* typed = reinterpret_cast<CompT*>(ptr);
        luabridge::push(L, typed);
        };

    s_registered[compName] = true;
}

void ScriptSystem::Initialise(ECSManager& ecsManager)
{
    m_ecs = &ecsManager;
    Scripting::Init();

    // --- LuaBridge registration ---
    lua_State* L = Scripting::GetLuaState();
    if (L) {
        luaL_checkversion(L);    // optional safety

        // Perform bindings once per process/module
        if (!g_luaBindingsDone)
        {
            // ---- 1) First pass: LuaBridge type registrations + register getters + pushers ----
            #define BEGIN_COMPONENT(CppType, LuaName) \
            { \
                const char* _compName = LuaName; \
                RegisterCompGetter<CppType>(_compName); \
                RegisterCompPusher<CppType>(_compName); \
                luabridge::getGlobalNamespace(L).beginClass<CppType>(_compName)

                #define PROPERTY(LuaFieldName, MemberPtr) \
                .addProperty(LuaFieldName, MemberPtr, MemberPtr)

                #define END_COMPONENT() \
                .endClass(); \
                g_luaRegisteredComponents_global.insert(_compName); \
            }

            // Include the single edit file (your editable component/property list)
            #include "LuaComponentBindings.inc"

            #undef BEGIN_COMPONENT
            #undef PROPERTY
            #undef END_COMPONENT

            // ---- 2) Second pass: Build the global Components metadata table in Lua ----
            lua_newtable(L); // Components table

            #define BEGIN_COMPONENT(CppType, LuaName) \
            { \
                const char* _compName = LuaName; \
                lua_pushstring(L, _compName); /* key */ \
                lua_newtable(L); /* value: metadata table for this component */

                #define PROPERTY(LuaFieldName, MemberPtr) \
                lua_pushstring(L, LuaFieldName); \
                lua_setfield(L, -2, LuaFieldName);

                #define END_COMPONENT() \
                lua_settable(L, -3); /* Components[_compName] = metadata_table */ \
            }

            #include "LuaComponentBindings.inc"

            #undef BEGIN_COMPONENT
            #undef PROPERTY
            #undef END_COMPONENT

            // Set global Components = { ... }
            lua_setglobal(L, "Components");

            g_luaBindingsDone = true;
        }

        // Copy registered names into the ScriptSystem instance set so HostGetComponentHandler can use them
        for (const auto& n : g_luaRegisteredComponents_global)
        {
            m_luaRegisteredComponents.insert(n);
        }

        // install host get-component handler that uses ComponentRegistry
        Scripting::SetHostGetComponentHandler([this](lua_State* L, uint32_t entityId, const std::string& compName) -> bool {
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] HostGetComponentHandler asked for comp=", compName, " entity=", entityId);

            // Check if component type is registered
            if (!ComponentRegistry::Instance().Has(compName))
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Component '", compName, "' not registered in ComponentRegistry");
                lua_pushnil(L);
                return true;
            }

            // Get the getter function
            auto getter = ComponentRegistry::Instance().GetGetter(compName);
            if (!getter)
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] No getter function for '", compName, "'");
                lua_pushnil(L);
                return true;
            }

            // Call the getter
            void* compPtr = getter(m_ecs, static_cast<Entity>(entityId));
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] Getter returned ptr=", compPtr, " for comp=", compName, " entity=", entityId);

            if (!compPtr)
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Component '", compName, "' not found on entity ", entityId, " (getter returned null)");
                lua_pushnil(L);
                return true;
            }

            // Try to find an automatic pusher for this component type
            auto pusherIt = g_componentPushers.find(compName);
            if (pusherIt != g_componentPushers.end())
            {
                // call the type-specific pusher that was registered when the .inc was processed
                pusherIt->second(L, compPtr);
                return true;
            }

            // fallback: push proxy userdata
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] Pushing component proxy for ", compName);
            PushComponentProxy(L, m_ecs, static_cast<Entity>(entityId), compName);
            return true;
            });

        // Only set a disk fallback reader if nobody registered a FS callback earlier.
        static bool s_fsRegistered = false;
        if (!s_fsRegistered)
        {
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
}

void ScriptSystem::Update(float dt, ECSManager& ecsManager)
{
    // advance coroutines & runtime tick if runtime initialized
    if (Scripting::GetLuaState()) Scripting::Tick(dt);

    // iterate over entities matched to this system (System::entities)
    for (Entity e : entities)
    {
        ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
        if (!comp || !comp->enabled) continue;

        if (!EnsureInstanceForEntity(e, ecsManager)) continue;

        // call instance Update(dt) via runtime object's public API
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_runtimeMap.find(e);
            if (it != m_runtimeMap.end() && it->second)
            {
                it->second->Update(dt);
            }
        }
    }

    // cleanup runtime instances for entities that no longer belong to this system
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::vector<Entity> toRemove;
        toRemove.reserve(8);
        for (auto& p : m_runtimeMap)
        {
            Entity e = p.first;
            if (entities.find(e) == entities.end()) toRemove.push_back(e);
        }
        for (Entity e : toRemove)
        {
            DestroyInstanceForEntity(e);
        }
    }
}

void ScriptSystem::Shutdown()
{
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
    if (m_ecs)
    {
        for (Entity e : entities)
        {
            ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
            if (sc)
            {
                sc->instanceCreated = false;
                sc->instanceId = -1;
            }
        }
    }

    Scripting::Shutdown();

    ENGINE_PRINT("[ScriptSystem] Shutdown complete\n");
}

bool ScriptSystem::EnsureInstanceForEntity(Entity e, ECSManager& ecsManager)
{
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return false;

    // Fast-path: if runtime already created, update POD and return
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second)
        {
            comp->instanceId = it->second->GetInstanceRef();
            comp->instanceCreated = true;
            return true;
        }
    }

    // Must ensure Lua runtime available
    if (!Scripting::GetLuaState())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] runtime missing; cannot create script for entity ", e, "\n");
        return false;
    }

    if (comp->scriptPath.empty())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] empty scriptPath for entity ", e, "\n");
        return false;
    }

    // Create and initialize a new runtime ScriptComponent *without holding the system mutex*
    std::unique_ptr<Scripting::ScriptComponent> runtimeComp = std::make_unique<Scripting::ScriptComponent>();

    // Attach script (this touches Lua). Do it outside the ScriptSystem mutex.
    if (!runtimeComp->AttachScript(comp->scriptPath))
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] AttachScript failed for ", comp->scriptPath.c_str(), " entity=", e, "\n");
        return false;
    }

    // Optionally register preserve keys (uses Scripting API and should be safe now)
    if (!comp->preserveKeys.empty())
    {
        Scripting::RegisterInstancePreserveKeys(runtimeComp->GetInstanceRef(), comp->preserveKeys);
    }

    // Try to bind instance to entity (runtime helper will set entityId + GetComponent)
    // Prefer runtime glue BindInstanceToEntity; fallback to manual set if it fails.
    bool bound = false;
    try
    {
        bound = Scripting::BindInstanceToEntity(runtimeComp->GetInstanceRef(), static_cast<uint32_t>(e));
    }
    catch (...)
    {
        bound = false;
    }

    if (!bound)
    {
        // fallback: set entityId field on instance table
        lua_State* L = ::Scripting::GetLuaState();
        if (L)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, runtimeComp->GetInstanceRef()); // push instance
            if (lua_istable(L, -1))
            {
                lua_pushinteger(L, static_cast<lua_Integer>(e));
                lua_setfield(L, -2, "entityId");
            }
            lua_pop(L, 1);
        }
    }

    // If the ScriptComponentData contains pending serialized state, apply it now (safe)
    if (!comp->pendingInstanceState.empty())
    {
        bool ok = false;
        try
        {
            ok = runtimeComp->DeserializeState(comp->pendingInstanceState);
        }
        catch (...)
        {
            ok = false;
        }
        if (ok)
        {
            comp->pendingInstanceState.clear();
        }
        else
        {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Failed to deserialize pending script state for entity ", e, "\n");
        }
    }

    // Move runtimeComp into map and update POD under lock
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_runtimeMap[e] = std::move(runtimeComp);
        Scripting::ScriptComponent* scPtr = m_runtimeMap[e].get();
        comp->instanceId = scPtr ? scPtr->GetInstanceRef() : LUA_NOREF;
        comp->instanceCreated = (scPtr != nullptr);
    }

    // Finally, call lifecycle Awake/Start outside the mutex to avoid lock inversion with Lua runtime.
    {
        Scripting::ScriptComponent* scPtr = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_runtimeMap.find(e);
            if (it != m_runtimeMap.end()) scPtr = it->second.get();
        }
        if (scPtr)
        {
            // These calls may call back into engine or use runtime; don't hold ScriptSystem lock while doing them
            scPtr->Awake();
            scPtr->Start();
        }
    }

    return true;
}

void ScriptSystem::DestroyInstanceForEntity(Entity e)
{
    std::unique_ptr<Scripting::ScriptComponent> runtimePtr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it == m_runtimeMap.end()) return;
        runtimePtr = std::move(it->second);
        m_runtimeMap.erase(it);
    }

    if (runtimePtr)
    {
        if (Scripting::GetLuaState()) runtimePtr->OnDisable();
        // runtimePtr destructor runs automatically when it goes out of scope
    }

    if (m_ecs)
    {
        ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
        if (sc) { sc->instanceCreated = false; sc->instanceId = -1; }
    }
}

void ScriptSystem::ReloadScriptForEntity(Entity e, ECSManager& ecsManager)
{
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return;

    if (!Scripting::GetLuaState())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Cannot reload: scripting runtime missing\n");
        return;
    }

    std::string preservedJson;

    // extract state then destroy old runtime
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second)
        {
            if (!comp->preserveKeys.empty()) preservedJson = it->second->SerializeState();
            if (Scripting::GetLuaState()) it->second->OnDisable();
            m_runtimeMap.erase(it);
        }
    }

    comp->instanceCreated = false;
    comp->instanceId = -1;

    // create new instance
    if (!EnsureInstanceForEntity(e, ecsManager))
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Reload failed to create new instance for entity ", e, "\n");
        return;
    }

    // reinject preserved state
    if (!preservedJson.empty())
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end() && it->second)
        {
            it->second->DeserializeState(preservedJson);
        }
    }
}

bool ScriptSystem::CallEntityFunction(Entity e, const std::string& funcName, ECSManager& ecsManager)
{
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return false;

    if (!comp->instanceCreated)
    {
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

ScriptComponentData* ScriptSystem::GetScriptComponent(Entity e, ECSManager& ecsManager)
{
    try
    {
        if (!ecsManager.HasComponent<ScriptComponentData>(e)) return nullptr;
        return &ecsManager.GetComponent<ScriptComponentData>(e);
    }
    catch (...)
    {
        return nullptr;
    }
}

const ScriptComponentData* ScriptSystem::GetScriptComponentConst(Entity e, const ECSManager& ecsManager) const
{
    // Delegate to the non-const implementation to avoid duplicating logic.
    // We must const_cast 'this' because GetScriptComponent is non-const;
    // this is safe here because GetScriptComponent does not mutate ScriptSystem state.
    ECSManager& nonConstEcs = const_cast<ECSManager&>(ecsManager);
    return const_cast<ScriptSystem*>(this)->GetScriptComponent(e, nonConstEcs);
}