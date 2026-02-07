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
#include <Animation/LuaAnimationComponent.hpp>

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

static AnimationComponent* Lua_FindAnimatorByName(const std::string& name)
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
            if (ecs.HasComponent<AnimationComponent>(e))
            {
                return &ecs.GetComponent<AnimationComponent>(e);
            }

            // Name matched but no Transform; stop searching if names are unique
            break;
        }
    }

    return nullptr;
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

static std::tuple<float, float, float> Lua_GetTransformWorldPosition(Transform* t)
{
    if (!t)
    {
        // Return something reasonable; Lua will get three numbers
        return std::make_tuple(0.0f, 0.0f, 0.0f);
    }

    const auto& p = t->worldPosition; // or global/world position if you have it
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


static void Lua_CreateEntityDup(const std::string& source_name, const std::string& base_name, int numofdupes)
{
    if (!g_ecsManager) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] ECSManager is null");
        return;
    }

    ECSManager& ecs = *g_ecsManager;

    // Find source entity by name
    Entity sourceEntity = static_cast<Entity>(-1);
    for (const auto& entity : ecs.GetActiveEntities()) {
        if (ecs.HasComponent<NameComponent>(entity)) {
            if (ecs.GetComponent<NameComponent>(entity).name == source_name) {
                sourceEntity = entity;
                break;
            }
        }
    }

    if (sourceEntity == static_cast<Entity>(-1)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Source entity '" + source_name + "' not found");
        return;
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ScriptSystem] Duplicating '" + source_name + "' " + std::to_string(numofdupes) + " times");

    // Create duplicates
    for (int i = 0; i < numofdupes; ++i) {
        try {
            // Generate unique name
            std::string newName = base_name;
            int counter = i + 1;
            bool nameExists = true;

            while (nameExists) {
                newName = base_name + std::to_string(counter);
                nameExists = false;

                // Check if name already exists
                for (const auto& entity : ecs.GetActiveEntities()) {
                    if (ecs.HasComponent<NameComponent>(entity)) {
                        if (ecs.GetComponent<NameComponent>(entity).name == newName) {
                            nameExists = true;
                            counter++;
                            break;
                        }
                    }
                }
            }

            // Create new entity
            Entity newEntity = ecs.CreateEntity();
            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ScriptSystem] Creating entity '" + newName + "' (ID: " + std::to_string(newEntity) + ")");

            // Set name
            if (ecs.HasComponent<NameComponent>(newEntity)) {
                ecs.GetComponent<NameComponent>(newEntity).name = newName;
            }

            // Copy Transform
            if (ecs.HasComponent<Transform>(sourceEntity)) {
                Transform sourceTransform = ecs.GetComponent<Transform>(sourceEntity);
                if (ecs.HasComponent<Transform>(newEntity)) {
                    ecs.GetComponent<Transform>(newEntity) = sourceTransform;
                }
                else {
                    ecs.AddComponent<Transform>(newEntity, sourceTransform);
                }
            }

            // Copy ActiveComponent
            if (ecs.HasComponent<ActiveComponent>(sourceEntity)) {
                ActiveComponent sourceActive = ecs.GetComponent<ActiveComponent>(sourceEntity);
                ecs.AddComponent<ActiveComponent>(newEntity, sourceActive);
            }

            // Copy ModelRenderComponent
            if (ecs.HasComponent<ModelRenderComponent>(sourceEntity)) {
                ModelRenderComponent sourceModel = ecs.GetComponent<ModelRenderComponent>(sourceEntity);
                ecs.AddComponent<ModelRenderComponent>(newEntity, sourceModel);
            }

            // Copy SpriteRenderComponent
            if (ecs.HasComponent<SpriteRenderComponent>(sourceEntity)) {
                SpriteRenderComponent sourceSprite = ecs.GetComponent<SpriteRenderComponent>(sourceEntity);
                ecs.AddComponent<SpriteRenderComponent>(newEntity, sourceSprite);
            }

            // Copy TextRenderComponent
            if (ecs.HasComponent<TextRenderComponent>(sourceEntity)) {
                TextRenderComponent sourceText = ecs.GetComponent<TextRenderComponent>(sourceEntity);
                ecs.AddComponent<TextRenderComponent>(newEntity, sourceText);
            }

            // Copy LightComponent
            if (ecs.HasComponent<LightComponent>(sourceEntity)) {
                LightComponent sourceLight = ecs.GetComponent<LightComponent>(sourceEntity);
                ecs.AddComponent<LightComponent>(newEntity, sourceLight);
            }

            // Copy CameraComponent
            if (ecs.HasComponent<CameraComponent>(sourceEntity)) {
                CameraComponent sourceCam = ecs.GetComponent<CameraComponent>(sourceEntity);
                // Don't copy active status for cameras (Unity-like)
                sourceCam.isActive = false;
                ecs.AddComponent<CameraComponent>(newEntity, sourceCam);
            }

            // Copy AudioComponent
            if (ecs.HasComponent<AudioComponent>(sourceEntity)) {
                AudioComponent sourceAudio = ecs.GetComponent<AudioComponent>(sourceEntity);
                ecs.AddComponent<AudioComponent>(newEntity, sourceAudio);
            }

            // Copy AnimationComponent
            if (ecs.HasComponent<AnimationComponent>(sourceEntity)) {
                AnimationComponent sourceAnim = ecs.GetComponent<AnimationComponent>(sourceEntity);
                ecs.AddComponent<AnimationComponent>(newEntity, sourceAnim);

                // Re-link animator to model if both exist
                if (ecs.HasComponent<ModelRenderComponent>(newEntity)) {
                    auto& modelComp = ecs.GetComponent<ModelRenderComponent>(newEntity);
                    auto& animComp = ecs.GetComponent<AnimationComponent>(newEntity);
                    if (modelComp.model && !animComp.clipPaths.empty()) {
                        Animator* animator = animComp.EnsureAnimator();
                        modelComp.SetAnimator(animator);
                        animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount(), newEntity);
                    }
                }
            }

            // Copy RigidBodyComponent
            if (ecs.HasComponent<RigidBodyComponent>(sourceEntity)) {
                RigidBodyComponent sourceRB = ecs.GetComponent<RigidBodyComponent>(sourceEntity);
                ecs.AddComponent<RigidBodyComponent>(newEntity, sourceRB);
            }

            // Copy ColliderComponent
            if (ecs.HasComponent<ColliderComponent>(sourceEntity)) {
                ColliderComponent sourceCollider = ecs.GetComponent<ColliderComponent>(sourceEntity);
                ecs.AddComponent<ColliderComponent>(newEntity, sourceCollider);
            }

            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ScriptSystem] Successfully created duplicate '" + newName + "'");
        }
        catch (const std::exception& e) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Failed to create duplicate " + std::to_string(i) + ": " + std::string(e.what()));
        }
    }
}
static void Lua_DestroyEntityDup(const std::string& base_name, int numToDestroy)
{
    if (!g_ecsManager) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] ECSManager is null");
        return;
    }

    ECSManager& ecs = *g_ecsManager;

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ScriptSystem] Destroying up to " + std::to_string(numToDestroy) + " entities with base name '" + base_name + "'");

    std::vector<Entity> entitiesToDestroy;

    // Find all entities matching the pattern
    for (const auto& entity : ecs.GetActiveEntities()) {
        if (ecs.HasComponent<NameComponent>(entity)) {
            std::string entityName = ecs.GetComponent<NameComponent>(entity).name;

            // Check if name starts with base_name and has a number
            if (entityName.find(base_name) == 0) {
                // Try to extract the number suffix
                std::string suffix = entityName.substr(base_name.length());

                // Check if suffix is a valid number
                if (!suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
                    entitiesToDestroy.push_back(entity);
                }
            }
        }
    }

    // Destroy the requested number of entities
    int destroyedCount = 0;
    int toDestroy = (numToDestroy <= 0) ? entitiesToDestroy.size() : std::min(numToDestroy, static_cast<int>(entitiesToDestroy.size()));

    for (int i = 0; i < toDestroy; ++i) {
        Entity entity = entitiesToDestroy[i];

        try {
            std::string entityName = "Unknown";
            if (ecs.HasComponent<NameComponent>(entity)) {
                entityName = ecs.GetComponent<NameComponent>(entity).name;
            }

            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ScriptSystem] Destroying entity '" + entityName + "' (ID: " + std::to_string(entity) + ")");

            ecs.DestroyEntity(entity);
            destroyedCount++;
        }
        catch (const std::exception& e) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Failed to destroy entity: " + std::string(e.what()));
        }
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ScriptSystem] Destroyed " + std::to_string(destroyedCount) + " entities");
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

// stack args: 1 = tag (string), 2 = maxResults (optional number)
static int Lua_GetEntitiesByTag(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    int maxResults = static_cast<int>(luaL_optinteger(L, 2, 4));

    if (!g_ecsManager) {
        lua_newtable(L);  // Return empty table instead of count=0
        return 1;
    }

    ECSManager& ecs = *g_ecsManager;
    const auto& entities = ecs.GetActiveEntities();

    std::vector<Entity> results;
    results.reserve(maxResults);

    for (Entity e : entities)
    {
        if (results.size() >= static_cast<size_t>(maxResults)) break;
        if (!ecs.HasComponent<TagComponent>(e)) continue;
        auto& tc = ecs.GetComponent<TagComponent>(e);
        if (tc.HasTag(tag))
        {
            results.push_back(e);
        }
    }

    // Create and populate a Lua table
    lua_createtable(L, results.size(), 0);  // Pre-allocate array part
    for (size_t i = 0; i < results.size(); ++i)
    {
        lua_pushinteger(L, static_cast<lua_Integer>(results[i]));
        lua_rawseti(L, -2, i + 1);  // Lua arrays are 1-indexed
    }

    return 1;  // Return the table
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

            // REGISTER THE PROXY CLASS SPECIALLY FOR ANIMATION COMPONENT
            luabridge::getGlobalNamespace(L)
                .beginClass<LuaAnimationComponent>("LuaAnimationComponent")
                .addConstructor<void(*)(Entity)>()
                .addFunction("Play", &LuaAnimationComponent::Play)
                .addFunction("Stop", &LuaAnimationComponent::Stop)
                .addFunction("Pause", &LuaAnimationComponent::Pause)
                .addFunction("PlayClip", &LuaAnimationComponent::PlayClip)
                .addFunction("SetSpeed", &LuaAnimationComponent::SetSpeed)
                .addFunction("SetBool", &LuaAnimationComponent::SetBool)
                .addFunction("SetTrigger", &LuaAnimationComponent::SetTrigger)
                .addFunction("SetFloat", &LuaAnimationComponent::SetFloat)
                .addFunction("SetInt", &LuaAnimationComponent::SetInt)
                .addFunction("GetCurrentState", &LuaAnimationComponent::GetCurrentState)
                .addFunction("IsPlaying", &LuaAnimationComponent::IsPlaying)
                .endClass();

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
            //ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] HostGetComponentHandler asked for comp=", compName, " entity=", entityId);

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
            //ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] Getter returned ptr=", compPtr, " for comp=", compName, " entity=", entityId);

            if (!compPtr)
            {
                ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ScriptSystem] Component '", compName, "' not found on entity ", entityId, " (getter returned null)");
                lua_pushnil(L);
                return true;
            }

            // [NEW] SPECIAL CASE: ANIMATION PROXY
            if (compName == "AnimationComponent")
            {
                // Create the proxy on the stack
                LuaAnimationComponent proxy(static_cast<Entity>(entityId));
                luabridge::push(L, proxy);
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
    // Use scaled delta time so coroutines respect pause state
    if (Scripting::GetLuaState()) Scripting::Tick(static_cast<float>(TimeManager::GetDeltaTime()));

    // ==========================================================================
    // FIX: Create ALL script instances first before calling any Awake/Start.
    // This ensures that when any script's Start() calls Engine.GetEntityByName()
    // and tries to interact with another entity's script, that script exists.
    // Previously, instances were created in EntityID order during the Update loop,
    // so scripts with lower IDs couldn't find scripts with higher IDs during Start().
    // ==========================================================================

    // Phase 1: Create instances for all entities (no Awake/Start yet)
    std::vector<Entity> newlyCreatedEntities;
    for (Entity e : entities)
    {
        if (!m_ecs->IsEntityActiveInHierarchy(e)) {
            continue;
        }

        ScriptComponentData* comp = GetScriptComponent(e, *m_ecs);
        if (!comp) continue;

        // Check if this entity needs new instances created
        bool needsCreation = false;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_runtimeMap.find(e);
            if (it == m_runtimeMap.end()) {
                needsCreation = true;
            } else {
                // Check if all scripts have instances
                for (size_t i = 0; i < comp->scripts.size(); ++i) {
                    if (comp->scripts[i].enabled && !comp->scripts[i].scriptPath.empty()) {
                        if (i >= it->second.size() || !it->second[i]) {
                            needsCreation = true;
                            break;
                        }
                    }
                }
            }
        }

        if (needsCreation) {
            if (EnsureInstanceForEntityNoLifecycle(e, *m_ecs)) {
                newlyCreatedEntities.push_back(e);
            }
        }
    }

    // Phase 2: Call Awake on all newly created instances
    for (Entity e : newlyCreatedEntities)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (auto& scriptInst : it->second)
            {
                if (scriptInst)
                {
                    scriptInst->Awake();
                }
            }
        }
    }

    // Phase 3: Call Start on all newly created instances
    for (Entity e : newlyCreatedEntities)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (auto& scriptInst : it->second)
            {
                if (scriptInst)
                {
                    scriptInst->Start();
                }
            }
        }
    }

    // Phase 4: Update all entities
    for (Entity e : entities)
    {
        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!m_ecs->IsEntityActiveInHierarchy(e)) {
            continue;
        }

        ScriptComponentData* comp = GetScriptComponent(e, *m_ecs);
        if (!comp) continue;

        // call Update(dt) on all script instances for this entity
        // Use scaled delta time so scripts receive dt=0 when paused
        // UI scripts that need to run during pause should use Time.GetUnscaledDeltaTime() directly
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

    g_ecsManager = nullptr;
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

// Creates script instances WITHOUT calling Awake/Start.
// Used by the phased Update() to ensure all instances exist before any lifecycle callbacks.
bool ScriptSystem::EnsureInstanceForEntityNoLifecycle(Entity e, ECSManager& ecsManager)
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

    bool anyCreated = false;

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

        anyCreated = true;
        // NOTE: Awake/Start are NOT called here - caller is responsible for calling them
    }

    return anyCreated;
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

    // ==========================================================================
    // PHASED RELOAD: Create all instances first, then call Awake/Start
    // This ensures all script instances exist before any lifecycle callbacks run,
    // fixing the bug where scripts with lower EntityIDs couldn't find scripts
    // with higher EntityIDs during their Awake/Start.
    // ==========================================================================

    // Phase 1: Collect preserved states and destroy old instances
    std::unordered_map<Entity, std::vector<std::string>> preservedStatesMap;
    std::vector<Entity> entitiesToReload;

    for (Entity e : entitySnapshot)
    {
        ScriptComponentData* comp = GetScriptComponent(e, *m_ecs);
        if (!comp)
        {
            // If the entity lost its script component, ensure runtime is cleared
            DestroyInstanceForEntity(e);
            continue;
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

        preservedStatesMap[e] = std::move(preservedStates);
        entitiesToReload.push_back(e);
    }

    // Phase 2: Create all instances WITHOUT calling Awake/Start
    for (Entity e : entitiesToReload)
    {
        if (!EnsureInstanceForEntityNoLifecycle(e, *m_ecs))
        {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] Reload failed to create new instances for entity ", e, "\n");
        }
    }

    // Phase 3: Reinject preserved states (before Awake/Start so state is available)
    for (Entity e : entitiesToReload)
    {
        auto statesIt = preservedStatesMap.find(e);
        if (statesIt == preservedStatesMap.end()) continue;

        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (size_t i = 0; i < it->second.size() && i < statesIt->second.size(); ++i)
            {
                if (!statesIt->second[i].empty() && it->second[i])
                {
                    it->second[i]->DeserializeState(statesIt->second[i]);
                }
            }
        }
    }

    // Phase 4: Call Awake on all instances
    for (Entity e : entitiesToReload)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (auto& scriptInst : it->second)
            {
                if (scriptInst)
                {
                    scriptInst->Awake();
                }
            }
        }
    }

    // Phase 5: Call Start on all instances
    for (Entity e : entitiesToReload)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (auto& scriptInst : it->second)
            {
                if (scriptInst)
                {
                    scriptInst->Start();
                }
            }
        }
        NotifyInstancesChanged(e);
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

// Create a fresh ephemeral instance from file, bind it to targetEntity, call the function, then destroy it.
// This is useful for UI callbacks that need entity context (instance:GetComponent).
bool ScriptSystem::CallStandaloneScriptFunctionWithEntity(const std::string& scriptPath, const std::string& scriptGuidStr, const std::string& funcName, Entity targetEntity)
{
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] CallStandaloneScriptFunctionWithEntity START: script=", scriptPath, " fn=", funcName);
    if (scriptPath.empty() || funcName.empty()) {
        return false;
    }

    if (!Scripting::GetLuaState()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ScriptSystem] Cannot call standalone function with entity: Lua runtime not available");
        return false;
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] Creating ephemeral instance from file");
    int instRef = Scripting::CreateInstanceFromFile(scriptPath);
    if (instRef == LUA_NOREF) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ScriptSystem] Cannot create ephemeral instance for ", scriptPath.c_str());
        return false;
    }
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] Created ephemeral instance: instRef=", instRef);

    // Bind instance to target entity so that instance:GetComponent works
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] Binding instance to entity ", targetEntity);
    if (!Scripting::BindInstanceToEntity(instRef, static_cast<uint32_t>(targetEntity))) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ScriptSystem] Failed to bind ephemeral instance to entity ", targetEntity);
        // proceed anyway
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] Calling function: ", funcName);
    bool success = Scripting::CallInstanceFunction(instRef, funcName);
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] Function call completed: success=", success);

    if (!success) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ScriptSystem] Ephemeral call failed: ", funcName.c_str(), " on script ", scriptPath.c_str());
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] About to destroy ephemeral instance");
    Scripting::DestroyInstance(instRef);
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[DEBUG] CallStandaloneScriptFunctionWithEntity END");

    return success;
}