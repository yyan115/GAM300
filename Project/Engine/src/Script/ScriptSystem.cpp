// ScriptSystem.cpp
#include "pch.h"
#include "Script/ScriptSystem.hpp"
#include "ECS/ECSManager.hpp"
#include "Script/ScriptComponentData.hpp"
#include "Script/LuaBindableComponents.hpp"
#include "Script/LuaBindableSystems.hpp"
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
            // ============================================================================
            // COMPONENT BINDINGS (existing code)
            // ============================================================================

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

            #include "LuaComponentBindings.inc"
            
            #undef BEGIN_COMPONENT
            #undef PROPERTY
            #undef END_COMPONENT

            // ---- Second pass: Components metadata table ----
            lua_newtable(L);

            #define BEGIN_COMPONENT(CppType, LuaName) \
            { \
                const char* _compName = LuaName; \
                lua_pushstring(L, _compName); \
                lua_newtable(L);

                #define PROPERTY(LuaFieldName, MemberPtr) \
                lua_pushstring(L, LuaFieldName); \
                lua_setfield(L, -2, LuaFieldName);

                #define END_COMPONENT() \
                lua_settable(L, -3); \
            }

            #include "LuaComponentBindings.inc"

            #undef BEGIN_COMPONENT
            #undef PROPERTY
            #undef END_COMPONENT

            lua_setglobal(L, "Components");

            // ============================================================================
            // SYSTEM BINDINGS (generic system framework)
            // ============================================================================
            
            // ---- Pass 1: Register all enums ----
            #define BEGIN_SYSTEM_NAMESPACE(Name) \
            { \
                auto _ns = luabridge::getGlobalNamespace(L).beginNamespace(Name);

                #define BEGIN_ENUM(EnumName) \
                _ns = _ns.beginNamespace(EnumName);
                
                #define ENUM_VALUE(LuaName, CppValue) \
                _ns = _ns.addVariable(LuaName, &CppValue, false);
                
                #define END_ENUM() \
                _ns = _ns.endNamespace();
                
                #define SYSTEM_FUNCTION(LuaName, CppFunction) \
                /* Skip functions in first pass */
                
                #define END_SYSTEM_NAMESPACE() \
                _ns.endNamespace(); \
            }

            #include "LuaSystemBindings.inc"

            #undef BEGIN_SYSTEM_NAMESPACE
            #undef BEGIN_ENUM
            #undef ENUM_VALUE
            #undef END_ENUM
            #undef SYSTEM_FUNCTION
            #undef END_SYSTEM_NAMESPACE

            // ---- Pass 2: Register all functions ----
            #define BEGIN_SYSTEM_NAMESPACE(Name) \
            { \
                auto _ns = luabridge::getGlobalNamespace(L).beginNamespace(Name);
            
                #define BEGIN_ENUM(EnumName) \
                /* Skip enums in second pass */
            
                #define ENUM_VALUE(LuaName, CppValue) \
                /* Skip enum values in second pass */
            
                #define END_ENUM() \
                /* Skip enum end in second pass */
            
                #define SYSTEM_FUNCTION(LuaName, CppFunction) \
                _ns = _ns.addFunction(LuaName, &CppFunction);
            
                #define END_SYSTEM_NAMESPACE() \
                _ns.endNamespace(); \
            }
            
            #include "LuaSystemBindings.inc"
            
            #undef BEGIN_SYSTEM_NAMESPACE
            #undef BEGIN_ENUM
            #undef ENUM_VALUE
            #undef END_ENUM
            #undef SYSTEM_FUNCTION
            #undef END_SYSTEM_NAMESPACE

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
        if (!comp) continue;

        if (!EnsureInstanceForEntity(e, ecsManager)) continue;

        // call Update(dt) on all script instances for this entity
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_runtimeMap.find(e);
            if (it != m_runtimeMap.end())
            {
                for (auto& scriptInst : it->second)
                {
                    if (scriptInst)
                    {
                        scriptInst->Update(dt);
                    }
                }
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
        auto& scriptVec = p.second;
        for (auto& ptr : scriptVec) {
            if (ptr) {
                if (Scripting::GetLuaState()) ptr->OnDisable();
                // unique_ptr cleanup will call destructor which frees Lua refs if possible
            }
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
                for (auto& script : sc->scripts)
                {
                    script.instanceCreated = false;
                    script.instanceId = -1;
                }
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

    // Must ensure Lua runtime available
    if (!Scripting::GetLuaState())
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] runtime missing; cannot create scripts for entity ", e, "\n");
        return false;
    }

    // Ensure we have a vector for this entity in the runtime map
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_runtimeMap.find(e) == m_runtimeMap.end())
        {
            m_runtimeMap[e] = std::vector<std::unique_ptr<Scripting::ScriptComponent>>();
        }
    }

    // Process each script in the component
    for (size_t scriptIdx = 0; scriptIdx < comp->scripts.size(); ++scriptIdx)
    {
        ScriptData& script = comp->scripts[scriptIdx];

        if (!script.enabled || script.scriptPath.empty()) continue;

        // Check if runtime instance already exists for this script
        bool alreadyExists = false;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto& scriptVec = m_runtimeMap[e];
            if (scriptIdx < scriptVec.size() && scriptVec[scriptIdx])
            {
                script.instanceId = scriptVec[scriptIdx]->GetInstanceRef();
                script.instanceCreated = true;
                alreadyExists = true;
            }
        }

        if (alreadyExists) continue;

        // Create new runtime instance
        auto runtimeComp = std::make_unique<Scripting::ScriptComponent>();

        if (!runtimeComp->AttachScript(script.scriptPath))
        {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] AttachScript failed for ", script.scriptPath.c_str(), " entity=", e, "\n");
            continue;
        }

        // Register preserve keys
        if (!script.preserveKeys.empty())
        {
            Scripting::RegisterInstancePreserveKeys(runtimeComp->GetInstanceRef(), script.preserveKeys);
        }

        // Bind to entity
        bool bound = false;
        try {
            bound = Scripting::BindInstanceToEntity(runtimeComp->GetInstanceRef(), static_cast<uint32_t>(e));
        } catch (...) { bound = false; }

        if (!bound)
        {
            lua_State* L = Scripting::GetLuaState();
            if (L)
            {
                lua_rawgeti(L, LUA_REGISTRYINDEX, runtimeComp->GetInstanceRef());
                if (lua_istable(L, -1))
                {
                    lua_pushinteger(L, static_cast<lua_Integer>(e));
                    lua_setfield(L, -2, "entityId");
                }
                lua_pop(L, 1);
            }
        }

        // Deserialize pending state
        if (!script.pendingInstanceState.empty())
        {
            bool ok = false;
            try {
                ok = runtimeComp->DeserializeState(script.pendingInstanceState);
            } catch (...) { ok = false; }

            if (ok)
            {
                script.pendingInstanceState.clear();
            }
            else
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Failed to deserialize pending state for script ", scriptIdx, " entity ", e, "\n");
            }
        }

        // Store in runtime map
        Scripting::ScriptComponent* scPtr = runtimeComp.get();
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto& scriptVec = m_runtimeMap[e];

            // Resize vector if needed
            if (scriptIdx >= scriptVec.size())
            {
                scriptVec.resize(scriptIdx + 1);
            }

            scriptVec[scriptIdx] = std::move(runtimeComp);
            script.instanceId = scPtr ? scPtr->GetInstanceRef() : LUA_NOREF;
            script.instanceCreated = (scPtr != nullptr);
        }

        // Call lifecycle methods
        if (scPtr)
        {
            scPtr->Awake();
            scPtr->Start();
        }
    }

    return true;
}

void ScriptSystem::DestroyInstanceForEntity(Entity e)
{
    std::vector<std::unique_ptr<Scripting::ScriptComponent>> runtimePtrs;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it == m_runtimeMap.end()) return;
        runtimePtrs = std::move(it->second);
        m_runtimeMap.erase(it);
    }

    for (auto& runtimePtr : runtimePtrs)
    {
        if (runtimePtr)
        {
            if (Scripting::GetLuaState()) runtimePtr->OnDisable();
            // runtimePtr destructor runs automatically
        }
    }

    if (m_ecs)
    {
        ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
        if (sc)
        {
            for (auto& script : sc->scripts)
            {
                script.instanceCreated = false;
                script.instanceId = -1;
            }
        }
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

    std::vector<std::string> preservedStates;

    // extract state then destroy old runtimes
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (size_t i = 0; i < it->second.size(); ++i)
            {
                auto& scriptInst = it->second[i];
                if (scriptInst)
                {
                    if (i < comp->scripts.size() && !comp->scripts[i].preserveKeys.empty())
                    {
                        preservedStates.push_back(scriptInst->SerializeState());
                    }
                    else
                    {
                        preservedStates.push_back("");
                    }

                    if (Scripting::GetLuaState()) scriptInst->OnDisable();
                }
            }
            m_runtimeMap.erase(it);
        }
    }

    // Reset all script instance flags
    for (auto& script : comp->scripts)
    {
        script.instanceCreated = false;
        script.instanceId = -1;
    }

    // create new instances
    if (!EnsureInstanceForEntity(e, ecsManager))
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Reload failed to create new instances for entity ", e, "\n");
        return;
    }

    // reinject preserved states
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (size_t i = 0; i < it->second.size() && i < preservedStates.size(); ++i)
            {
                if (!preservedStates[i].empty() && it->second[i])
                {
                    it->second[i]->DeserializeState(preservedStates[i]);
                }
            }
        }
    }
}

bool ScriptSystem::CallEntityFunction(Entity e, const std::string& funcName, ECSManager& ecsManager)
{
    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return false;

    if (!EnsureInstanceForEntity(e, ecsManager)) return false;

    if (!Scripting::GetLuaState()) return false;

    bool anySuccess = false;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            // Call the function on all scripts that have it
            for (auto& scriptInst : it->second)
            {
                if (scriptInst)
                {
                    int instRef = scriptInst->GetInstanceRef();
                    if (Scripting::CallInstanceFunction(instRef, funcName))
                    {
                        anySuccess = true;
                    }
                }
            }
        }
    }

    return anySuccess;
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