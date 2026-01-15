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
#include <TimeManager.hpp>

#include "Scripting.h"          // for public glue functions used
#include "ECS/NameComponent.hpp"    // or wherever NameComponent is defined
#include "Transform/TransformComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
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
static ECSManager* g_ecsManager = nullptr;

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
        // Cast to void to fix warning C4834 - discarding [[nodiscard]] return value
        // luabridge::push(L, typed);
        (void)luabridge::push(L, typed);
        };

    s_registered[compName] = true;
}

static Transform* Lua_FindTransformByName(const std::string& name)
{
    if (!g_ecsManager) return nullptr;
    ECSManager& ecs = *g_ecsManager;

    // Get all active entities (same pattern as InspectorPanel)
    const auto& entities = ecs.GetActiveEntities();

    for (Entity e : entities)
    {
        if (!ecs.HasComponent<NameComponent>(e))
            continue;

        auto& nc = ecs.GetComponent<NameComponent>(e);
        if (nc.name == name)
        {
            // Found the entity with matching name, now ensure it has a Transform
            if (ecs.HasComponent<Transform>(e))
            {
                return &ecs.GetComponent<Transform>(e);
            }

            // Name matched but no Transform; stop searching if names are unique
            break;
        }
    }

    return nullptr;
}

static std::tuple<float, float, float> Lua_GetTransformPosition(Transform* t)
{
    if (!t)
    {
        // Return something reasonable; Lua will get three numbers
        return std::make_tuple(0.0f, 0.0f, 0.0f);
    }

    const auto& p = t->localPosition; // or global/world position if you have it
    return std::make_tuple(p.x, p.y, p.z);
}

static std::tuple<float, float, float> Lua_GetTransformRotation(Transform* t)
{
    if (!t)
    {
        // Return something reasonable; Lua will get three numbers
        return std::make_tuple(0.0f, 0.0f, 0.0f);
    }

    const auto& p = t->localRotation; // or global/world position if you have it
    return std::make_tuple(p.x, p.y, p.z);
}


static Entity Lua_FindEntityByName(const std::string& name)
{
    if (!g_ecsManager) return -1; 
    ECSManager& ecs = *g_ecsManager;

    const auto& entities = ecs.GetActiveEntities();

    for (Entity e : entities)
    {
        if (!ecs.HasComponent<NameComponent>(e))
            continue;

        auto& nc = ecs.GetComponent<NameComponent>(e);
        if (nc.name == name)
        {
            return e;
        }
    }

    return -1; // Not found
}

static std::tuple<float, float> Lua_ScreenToGameCoordinates(float mouseX, float mouseY)
{
    float viewportWidth = static_cast<float>(WindowManager::GetViewportWidth());
    float viewportHeight = static_cast<float>(WindowManager::GetViewportHeight());
    
    int gameResWidth, gameResHeight;
    GraphicsManager::GetInstance().GetTargetGameResolution(gameResWidth, gameResHeight);
    
    // Map mouse coordinates from viewport space to game resolution space
    float gameX = (mouseX / viewportWidth) * static_cast<float>(gameResWidth);
    float gameY = static_cast<float>(gameResHeight) - (mouseY / viewportHeight) * static_cast<float>(gameResHeight);
    
    return std::make_tuple(gameX, gameY);
}




//TEMP FUNCTION TO BE CHANGED
static size_t Lua_FindCurrentClipByName(const std::string& name)
{
    if (!g_ecsManager) return -1;
    ECSManager& ecs = *g_ecsManager;

    // Get all active entities (same pattern as InspectorPanel)
    const auto& entities = ecs.GetActiveEntities();

    for (Entity e : entities)
    {
        if (!ecs.HasComponent<NameComponent>(e))
            continue;

        auto& nc = ecs.GetComponent<NameComponent>(e);
        if (nc.name == name)
        {
            // Found the entity with matching name, now ensure it has a Transform
            if (ecs.HasComponent<AnimationComponent>(e))
            {
                auto& animation = ecs.GetComponent<AnimationComponent>(e);
                return animation.GetActiveClipIndex();
            }

            // Name matched but no AnimationComponent; stop searching if names are unique
            break;
        }
    }

    return -1;
}









void ScriptSystem::Initialise(ECSManager& ecsManager)
{
    // DEBUG: This MUST print if new code is compiled - v3
    std::cout << "[ScriptSystem] ===== INITIALISE v3 =====" << std::endl;
    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] ===== INITIALISE v3 =====");

    m_ecs = &ecsManager;
	g_ecsManager = &ecsManager;

    Scripting::Init();

    //PHYSICSSYSTEM REFERENCE (TEMPORARY?)
    PhysicsSystemWrappers::g_PhysicsSystem = ecsManager.physicsSystem.get();

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

            #define METHOD(LuaName, CppMethod) \
                .addFunction(LuaName, CppMethod)

            #define END_COMPONENT() \
                .endClass(); \
                g_luaRegisteredComponents_global.insert(_compName); \
            }

            #include "Script/LuaComponentBindings.inc"
            
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

            // Undef to fix warning C4005 - macro redefinition
            #undef METHOD
            #define METHOD(...)

            #define END_COMPONENT() \
                lua_settable(L, -3); \
            }

            #include "Script/LuaComponentBindings.inc"
            #include <Asset Manager/AssetManager.hpp>

            #undef BEGIN_COMPONENT
            #undef PROPERTY
            #undef END_COMPONENT

            lua_setglobal(L, "Components");

            // ============================================================================
            // SYSTEM BINDINGS
            // ============================================================================

            #define BEGIN_SYSTEM(Name) \
                luabridge::getGlobalNamespace(L).beginNamespace(Name)

            #define BEGIN_CONSTANTS(Name) \
                .beginNamespace(Name)

            #define CONSTANT(LuaName, CppValue) \
                .addVariable(LuaName, static_cast<int>(CppValue))

            #define END_CONSTANTS() \
                .endNamespace()

            #define FUNCTION(LuaName, CppFunc) \
                .addFunction(LuaName, CppFunc)

            #define END_SYSTEM() \
                .endNamespace();

            // Force rebuild when LuaSystemBindings.inc changes - v2
            #include "Script/LuaSystemBindings.inc"

            // Debug: Verify Physics namespace was created
            lua_getglobal(L, "Physics");
            if (lua_isnil(L, -1)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] CRITICAL: Physics namespace not created!");
            } else {
                ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] Physics namespace created successfully");
            }
            lua_pop(L, 1);

            #undef BEGIN_SYSTEM
            #undef BEGIN_CONSTANTS
            #undef CONSTANT
            #undef END_CONSTANTS
            #undef FUNCTION
            #undef END_SYSTEM

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

        // mark that we need at least one reconcile on the first Update after Initialise / Play
        m_needsReconcile = true;

        //TODO add #if android to avoid bloating on editor
        // Initialize the scripts' paths from the GUID for android
#ifdef ANDROID
        for (const auto& entity : entities) {
            auto& scriptComp = ecsManager.GetComponent<ScriptComponentData>(entity);
            for (auto& script : scriptComp.scripts) {
                std::string scriptPath = AssetManager::GetInstance().GetAssetPathFromGUID(script.scriptGuid);
                if (!scriptPath.empty()) {
                    script.scriptPath = scriptPath.substr(scriptPath.find("Resources"));
                    ENGINE_LOG_DEBUG("[ScriptSystem] Current script path: " + script.scriptPath);
                }
            }
        }
#endif

        ENGINE_PRINT("[ScriptSystem] Initialised\n");
    }
}
void ScriptSystem::Update()
{
    // one-shot reconcile on first update after initialise/play
    if (m_needsReconcile && m_ecs)
    {
        m_needsReconcile = false;
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] One-shot reconcile: reloading all script instances");
        ReloadAllInstances();
    }

    // advance coroutines & runtime tick if runtime initialized
    if (Scripting::GetLuaState()) Scripting::Tick(static_cast<float>(TimeManager::GetDeltaTime()));

    // iterate over entities matched to this system (System::entities)
    for (Entity e : entities)
    {
        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!m_ecs->IsEntityActiveInHierarchy(e)) {
            continue;
        }

        ScriptComponentData* comp = GetScriptComponent(e, *m_ecs);
        if (!comp) continue;

        if (!EnsureInstanceForEntity(e, *m_ecs)) continue;

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
                        scriptInst->Update(static_cast<float>(TimeManager::GetDeltaTime()));
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

	g_ecsManager = nullptr;

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

    // Clean up standalone instances (used by ButtonComponent)
    for (auto& p : m_standaloneInstances) {
        if (p.second && Scripting::GetLuaState()) {
            p.second->OnDisable();
        }
    }
    m_standaloneInstances.clear();

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
    g_luaBindingsDone = false;

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

            if (!ok)
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Failed to deserialize pending state for script ", scriptIdx, " entity ", e, "\n");
            }
            // DO NOT clear pendingInstanceState - we need it to persist across multiple play/stop cycles
            // This ensures behavior where inspector edits are preserved
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
            // Notify listeners that instances for 'e' have been created/changed.
            NotifyInstancesChanged(e);
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
        NotifyInstancesChanged(e);
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
    
    NotifyInstancesChanged(e);
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

void ScriptSystem::ReloadSystem()
{
    Shutdown();
    Initialise(*m_ecs);
}

void ScriptSystem::ReloadAllInstances()
{
    // collect entity list snapshot under lock to avoid iterator invalidation
    std::vector<Entity> entitySnapshot;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        entitySnapshot.assign(entities.begin(), entities.end());
    }

    if (!m_ecs) return;

    for (Entity e : entitySnapshot)
    {
        // If the entity still has a ScriptComponentData, reload it; otherwise destroy any runtime instances
        ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
        if (sc)
        {
            // This will serialize/preserve state for preserveKeys and recreate instances
            ReloadScriptForEntity(e, *m_ecs);
        }
        else
        {
            // If the entity lost its script component, ensure runtime is cleared
            DestroyInstanceForEntity(e);
        }
    }
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

/***********************************************************************************************************/
// ---------------------------
// GetInstanceRefForScript
// ---------------------------
int ScriptSystem::GetInstanceRefForScript(Entity e, const std::string& scriptGuidStr)
{
    // Fast path: check the POD ScriptComponentData for instanceId (this is kept in sync by EnsureInstanceForEntity)
    ScriptComponentData* sc = GetScriptComponent(e, *m_ecs);
    if (sc)
    {
        for (size_t i = 0; i < sc->scripts.size(); ++i)
        {
            const ScriptData& sd = sc->scripts[i];
            if (sd.scriptGuidStr == scriptGuidStr)
            {
                if (sd.instanceCreated && sd.instanceId != -1) {
                    return sd.instanceId;
                }
                break;
            }
        }
    }

    // Fallback: inspect runtime map under lock (runtimeMap contains ScriptComponent instances)
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_runtimeMap.find(e);
    if (it == m_runtimeMap.end()) return LUA_NOREF;

    // Need to find which index in the component's scripts has that GUID
    if (!sc) return LUA_NOREF;
    for (size_t i = 0; i < sc->scripts.size(); ++i)
    {
        if (sc->scripts[i].scriptGuidStr == scriptGuidStr)
        {
            if (i < it->second.size() && it->second[i]) {
                return it->second[i]->GetInstanceRef();
            }
            return LUA_NOREF;
        }
    }
    return LUA_NOREF;
}

// ---------------------------
// CallInstanceFunctionByScriptGuid
// ---------------------------
bool ScriptSystem::CallInstanceFunctionByScriptGuid(Entity e, const std::string& scriptGuidStr, const std::string& funcName)
{
    if (!Scripting::GetLuaState()) return false;

    int instRef = GetInstanceRefForScript(e, scriptGuidStr);
    if (instRef == LUA_NOREF) return false;

    return Scripting::CallInstanceFunction(instRef, funcName);
}

// ---------------------------
// Register / Unregister callbacks
// ---------------------------
// NOTE: this design uses the std::function target pointer as a simple key so clients can compute
// the same key (as done in your ButtonComponent example). This works for plain function pointers.
// If you register lambdas with captures, target<void>() may be null � see comment below.
void ScriptSystem::RegisterInstancesChangedCallback(InstancesChangedCb cb)
{
    void* key = reinterpret_cast<void*>(cb.target<void>());
    // If target<void>() is null (e.g. capturing lambda), key will be nullptr.
    // That is acceptable with your current design as long as the client computes the same key.
    std::lock_guard<std::mutex> lk(m_mutex);
    m_instancesChangedCbs.emplace_back(key, std::move(cb));
}

void ScriptSystem::UnregisterInstancesChangedCallback(void* cbId)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_instancesChangedCbs.erase(
        std::remove_if(m_instancesChangedCbs.begin(), m_instancesChangedCbs.end(),
            [cbId](const auto& p) { return p.first == cbId; }),
        m_instancesChangedCbs.end());
}
// ---------------------------
// Instances-change helper
// ---------------------------
void ScriptSystem::NotifyInstancesChanged(Entity e)
{
    // LOGIC IS DISABLED FIRST
    // Copy callbacks under lock, then call outside lock to avoid reentrancy / deadlocks.
    //std::vector<InstancesChangedCb> callbacks;
    //{
    //    std::lock_guard<std::mutex> lk(m_mutex);
    //    callbacks.reserve(m_instancesChangedCbs.size());
    //    for (const auto& p : m_instancesChangedCbs) {
    //        callbacks.push_back(p.second);
    //    }
    //}

    //for (auto& cb : callbacks)
    //{
    //    try {
    //        if (cb) {  // Check if callback is valid
    //            cb(e);
    //        }
    //    }
    //    catch (const std::system_error& se) {
    //        // Mutex error - callback object was likely destroyed
    //        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
    //            "[ScriptSystem] Mutex error in callback for entity ", e, " - callback may be stale");
    //    }
    //    catch (...) {
    //        // swallow exceptions to avoid breaking engine flow
    //        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
    //            "[ScriptSystem] InstancesChanged callback threw for entity ", e);
    //    }
    //}
}

// ---------------------------
// Standalone Script Instances (for ButtonComponent callbacks)
// ---------------------------
int ScriptSystem::GetOrCreateStandaloneInstance(const std::string& scriptPath, const std::string& scriptGuidStr)
{
    if (scriptPath.empty() || scriptGuidStr.empty()) {
        return LUA_NOREF;
    }

    if (!Scripting::GetLuaState()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ScriptSystem] Cannot create standalone instance: Lua runtime not available");
        return LUA_NOREF;
    }

    std::lock_guard<std::mutex> lk(m_mutex);

    // Check if we already have an instance for this script
    auto it = m_standaloneInstances.find(scriptGuidStr);
    if (it != m_standaloneInstances.end() && it->second) {
        return it->second->GetInstanceRef();
    }

    // Create new instance
    auto runtimeComp = std::make_unique<Scripting::ScriptComponent>();

    if (!runtimeComp->AttachScript(scriptPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error,
            "[ScriptSystem] Failed to attach standalone script: ", scriptPath.c_str());
        return LUA_NOREF;
    }

    int instRef = runtimeComp->GetInstanceRef();

    // Cache the instance
    m_standaloneInstances[scriptGuidStr] = std::move(runtimeComp);

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[ScriptSystem] Created standalone script instance for: ", scriptPath.c_str(),
        " (GUID: ", scriptGuidStr.c_str(), ")");

    return instRef;
}

bool ScriptSystem::CallStandaloneScriptFunction(const std::string& scriptPath, const std::string& scriptGuidStr, const std::string& funcName)
{
    if (scriptPath.empty() || funcName.empty()) {
        return false;
    }

    int instRef = GetOrCreateStandaloneInstance(scriptPath, scriptGuidStr);
    if (instRef == LUA_NOREF) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ScriptSystem] Cannot call standalone function: no instance for ", scriptPath.c_str());
        return false;
    }

    bool success = Scripting::CallInstanceFunction(instRef, funcName);

    if (success) {
        ENGINE_PRINT(EngineLogging::LogLevel::Debug,
            "[ScriptSystem] Successfully called standalone function: ", funcName.c_str(),
            " on script ", scriptPath.c_str());
    }

    return success;
}