// ScriptSystem.cpp
#include "pch.h"
#include "Script/ScriptSystem.hpp"
#include "Script/LuaHeapReport.hpp"
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
#include <string_view>


// Define destructor where Scripting::ScriptComponent is a complete type
ScriptSystem::~ScriptSystem() = default;

namespace {
struct TransparentStringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};

constexpr std::array<std::string_view, 6> kTrackedIntEvents{{
    "OnTriggerEnter",
    "OnCollisionEnter",
    "OnTriggerExit",
    "OnCollisionExit",
    "OnTriggerStay",
    "OnCollisionStay",
}};
constexpr std::uint8_t kIntEventMaskKnown = 1u << 7;

constexpr std::uint8_t GetTrackedIntEventBit(std::string_view functionName) noexcept {
    for (std::size_t i = 0; i < kTrackedIntEvents.size(); ++i) {
        if (kTrackedIntEvents[i] == functionName) {
            return static_cast<std::uint8_t>(1u << i);
        }
    }
    return 0;
}
}

static std::unordered_map<
    std::string,
    std::function<void(lua_State*, void*)>,
    TransparentStringHash,
    std::equal_to<>> g_componentPushers;
static std::set<std::string> g_luaRegisteredComponents_global;
static bool g_luaBindingsDone = false;
static ECSManager* g_ecsManager = nullptr;
static std::unordered_map<
    std::string,
    Entity,
    TransparentStringHash,
    std::equal_to<>> g_entityByNameCache;

static Entity FindActiveEntityByName(ECSManager& ecs, const std::string& name)
{
    auto cached = g_entityByNameCache.find(name);
    if (cached != g_entityByNameCache.end()) {
        const Entity entity = cached->second;
        if (ecs.IsEntityAlive(entity)) {
            auto nameComponent = ecs.TryGetComponent<NameComponent>(entity);
            if (nameComponent && nameComponent->get().name == name) {
                return entity;
            }
        }
        g_entityByNameCache.erase(cached);
    }

    for (Entity entity : ecs.GetActiveEntitiesView())
    {
        auto nameComponent = ecs.TryGetComponent<NameComponent>(entity);
        if (nameComponent && nameComponent->get().name == name) {
            g_entityByNameCache.insert_or_assign(name, entity);
            return entity;
        }
    }
    return INVALID_ENTITY;
}

// Template to register component getters into the existing ComponentRegistry.
// This is idempotent per component type.
template<typename CompT>
static void RegisterCompGetter(const char* compName) {
    static std::unordered_map<std::string, bool> s_registered;
    if (s_registered[compName]) return;
    ComponentRegistry::Instance().Register<CompT>(compName,
        [](ECSManager* ecs, Entity e) -> CompT* {
            if (!ecs) return nullptr;
            auto component = ecs->TryGetComponent<CompT>(e);
            return component ? &component->get() : nullptr;
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

    const Entity entity = FindActiveEntityByName(ecs, name);
    if (entity == INVALID_ENTITY) return nullptr;
    auto animation = ecs.TryGetComponent<AnimationComponent>(entity);
    return animation ? &animation->get() : nullptr;
}

static Transform* Lua_FindTransformByName(const std::string& name)
{
    if (!g_ecsManager) return nullptr;
    ECSManager& ecs = *g_ecsManager;

    const Entity entity = FindActiveEntityByName(ecs, name);
    if (entity == INVALID_ENTITY) return nullptr;
    auto transform = ecs.TryGetComponent<Transform>(entity);
    return transform ? &transform->get() : nullptr;
}

static Transform* Lua_FindTransformByID(const Entity& id)
{
    if (!g_ecsManager) return nullptr;
    ECSManager& ecs = *g_ecsManager;

    if (!ecs.IsEntityAlive(id)) return nullptr;
    auto transform = ecs.TryGetComponent<Transform>(id);
    return transform ? &transform->get() : nullptr;
}

static AudioComponent* Lua_FindAudioCompByName(const std::string& name)
{
    if (!g_ecsManager) return nullptr;
    ECSManager& ecs = *g_ecsManager;

    const Entity entity = FindActiveEntityByName(ecs, name);
    if (entity == INVALID_ENTITY) return nullptr;
    auto audio = ecs.TryGetComponent<AudioComponent>(entity);
    return audio ? &audio->get() : nullptr;
}
static AudioComponent* Lua_FindAudioCompByID(const Entity& id)
{
    if (!g_ecsManager) return nullptr;
    ECSManager& ecs = *g_ecsManager;

    if (!ecs.IsEntityAlive(id)) return nullptr;
    auto audio = ecs.TryGetComponent<AudioComponent>(id);
    return audio ? &audio->get() : nullptr;
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

static int Lua_GetTransformPositionXYZ(lua_State* state)
{
    const auto result = luabridge::get<Transform*>(state, 1);
    const Transform* transform = result ? *result : nullptr;
    const Vector3D position = transform
        ? transform->localPosition
        : Vector3D(0.0f, 0.0f, 0.0f);

    lua_pushnumber(state, position.x);
    lua_pushnumber(state, position.y);
    lua_pushnumber(state, position.z);
    return 3;
}

static int Lua_GetTransformWorldPositionXYZ(lua_State* state)
{
    const auto result = luabridge::get<Transform*>(state, 1);
    const Transform* transform = result ? *result : nullptr;
    const Vector3D position = transform
        ? transform->worldPosition
        : Vector3D(0.0f, 0.0f, 0.0f);

    lua_pushnumber(state, position.x);
    lua_pushnumber(state, position.y);
    lua_pushnumber(state, position.z);
    return 3;
}

static int Lua_GetCharacterControllerPositionXYZ(lua_State* state)
{
    const auto result = luabridge::get<CharacterController*>(state, 1);
    CharacterController* controller = result ? *result : nullptr;
    const Vector3D position = controller
        ? controller->GetPosition()
        : Vector3D(0.0f, 0.0f, 0.0f);

    lua_pushnumber(state, position.x);
    lua_pushnumber(state, position.y);
    lua_pushnumber(state, position.z);
    return 3;
}

static int Lua_GetCharacterControllerVelocityXYZ(lua_State* state)
{
    const auto result = luabridge::get<CharacterController*>(state, 1);
    CharacterController* controller = result ? *result : nullptr;
    const Vector3D velocity = controller
        ? controller->GetVelocity()
        : Vector3D(0.0f, 0.0f, 0.0f);

    lua_pushnumber(state, velocity.x);
    lua_pushnumber(state, velocity.y);
    lua_pushnumber(state, velocity.z);
    return 3;
}

static int Lua_GetInputAxisXY(lua_State* state)
{
    const char* axisName = luaL_checkstring(state, 1);
    const glm::vec2 axis = g_inputManager
        ? g_inputManager->GetAxis(axisName)
        : glm::vec2(0.0f);

    lua_pushnumber(state, axis.x);
    lua_pushnumber(state, axis.y);
    return 2;
}

static int Lua_GetInputActionState(lua_State* state)
{
    const std::string action = luaL_checkstring(state, 1);
    const bool held = g_inputManager && g_inputManager->IsActionHeld(action);
    const bool pressed = g_inputManager && g_inputManager->IsActionPressed(action);
    const bool released = g_inputManager && g_inputManager->IsActionJustReleased(action);

    lua_pushboolean(state, held);
    lua_pushboolean(state, pressed);
    lua_pushboolean(state, released);
    return 3;
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

static Transform* Lua_GetTransformFromArray(lua_State* state, int transformsIndex, int index)
{
    lua_rawgeti(state, transformsIndex, index);
    const auto result = luabridge::get<Transform*>(state, -1);
    Transform* transform = result ? *result : nullptr;
    lua_pop(state, 1);
    return transform;
}

static int Lua_ApplyChainPositions(lua_State* state)
{
    if (!lua_istable(state, 1) || !lua_istable(state, 2) || !lua_isnumber(state, 3)) {
        lua_pushboolean(state, 0);
        return 1;
    }

    const int transformsIndex = lua_absindex(state, 1);
    const int positionsIndex = lua_absindex(state, 2);
    const int requestedCount = std::max(0, static_cast<int>(lua_tointeger(state, 3)));
    const int count = std::min({
        requestedCount,
        static_cast<int>(lua_rawlen(state, transformsIndex)),
        static_cast<int>(lua_rawlen(state, positionsIndex))});
    bool complete = count == requestedCount;

    for (int index = 1; index <= count; ++index) {
        Transform* transform = Lua_GetTransformFromArray(state, transformsIndex, index);
        ChainPhysicsWrappers::Detail::Point position;
        if (!transform || !ChainPhysicsWrappers::Detail::ReadPointAt(
                state, positionsIndex, index, position)) {
            complete = false;
            continue;
        }

        transform->localPosition.x = static_cast<float>(position.x);
        transform->localPosition.y = static_cast<float>(position.y);
        transform->localPosition.z = static_cast<float>(position.z);
        transform->isDirty = true;
    }

    lua_pushboolean(state, complete);
    return 1;
}

static int Lua_ApplyChainRotations(lua_State* state)
{
    if (!lua_istable(state, 1) || !lua_istable(state, 2) || !lua_isnumber(state, 3)) {
        lua_pushboolean(state, 0);
        return 1;
    }

    const int transformsIndex = lua_absindex(state, 1);
    const int rotationsIndex = lua_absindex(state, 2);
    const int requestedCount = std::max(0, static_cast<int>(lua_tointeger(state, 3)));
    const int count = std::min({
        requestedCount,
        static_cast<int>(lua_rawlen(state, transformsIndex)),
        static_cast<int>(lua_rawlen(state, rotationsIndex))});
    bool complete = count == requestedCount;

    for (int index = 1; index <= count; ++index) {
        Transform* transform = Lua_GetTransformFromArray(state, transformsIndex, index);
        lua_rawgeti(state, rotationsIndex, index);
        if (!transform || !lua_istable(state, -1)) {
            complete = false;
            lua_pop(state, 1);
            continue;
        }

        float values[4]{};
        bool valid = true;
        for (int component = 1; component <= 4; ++component) {
            lua_rawgeti(state, -1, component);
            if (!lua_isnumber(state, -1)) valid = false;
            values[component - 1] = static_cast<float>(lua_tonumber(state, -1));
            lua_pop(state, 1);
        }
        lua_pop(state, 1);
        if (!valid) {
            complete = false;
            continue;
        }

        transform->localRotation.w = values[0];
        transform->localRotation.x = values[1];
        transform->localRotation.y = values[2];
        transform->localRotation.z = values[3];
        transform->isDirty = true;
    }

    lua_pushboolean(state, complete);
    return 1;
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
    for (const auto& entity : ecs.GetActiveEntitiesView()) {
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
                for (const auto& entity : ecs.GetActiveEntitiesView()) {
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

static void Lua_DestroyEntity(Entity entity)
{
    if (!g_ecsManager) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[ScriptSystem] ECSManager is null");
        return;
	}

    // Add the entity to the entitiesPendingDestroy queue for deletion in the next ScriptSystem::Update.
    ECSManager& ecs = *g_ecsManager;
    ecs.scriptSystem->AddEntityPendingDestroy(entity);
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
    for (const auto& entity : ecs.GetActiveEntitiesView()) {
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
    if (!g_ecsManager) return INVALID_ENTITY;
    return FindActiveEntityByName(*g_ecsManager, name);
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
    const auto& entities = ecs.GetActiveEntitiesView();

    maxResults = std::max(maxResults, 0);
    lua_createtable(L, maxResults, 0);
    int resultCount = 0;

    for (Entity e : entities)
    {
        if (resultCount >= maxResults) break;
        auto tagComponent = ecs.TryGetComponent<TagComponent>(e);
        if (tagComponent && tagComponent->get().HasTag(tag))
        {
            lua_pushinteger(L, static_cast<lua_Integer>(e));
            lua_rawseti(L, -2, ++resultCount);
        }
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

    const Entity entity = FindActiveEntityByName(ecs, name);
    if (entity == INVALID_ENTITY) return static_cast<size_t>(-1);
    auto animation = ecs.TryGetComponent<AnimationComponent>(entity);
    return animation
        ? animation->get().GetActiveClipIndex()
        : static_cast<size_t>(-1);
}







void ScriptSystem::Initialise(ECSManager& ecsManager)
{
    // DEBUG: This MUST print if new code is compiled - v3
    //std::cout << "[ScriptSystem] ===== INITIALISE v3 =====" << std::endl;
    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] ===== INITIALISE v3 =====");

    m_ecs = &ecsManager;
	g_ecsManager = &ecsManager;
    g_entityByNameCache.clear();

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
                .addFunction("GetStateTime", &LuaAnimationComponent::GetStateTime)
                .addFunction("GetNormalizedTime", &LuaAnimationComponent::GetNormalizedTime)
                .addFunction("GetClipDuration", &LuaAnimationComponent::GetClipDuration)
                .addFunction("IsPlaying", &LuaAnimationComponent::IsPlaying)
                .addFunction("ResetSM", &LuaAnimationComponent::ResetSM)
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
        Scripting::SetHostGetComponentHandler([this](lua_State* L, uint32_t entityId, std::string_view compName) -> bool {
            //ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] HostGetComponentHandler asked for comp=", compName, " entity=", entityId);

            // Resolve metadata and the getter in one registry lookup.
            ComponentRegistry::ComponentInfo componentInfo;
            if (!ComponentRegistry::Instance().Get(compName, componentInfo))
            {
                ENGINE_PRINT(
                    EngineLogging::LogLevel::Warn,
                    "[ScriptSystem] Component '",
                    std::string(compName),
                    "' not registered in ComponentRegistry");
                lua_pushnil(L);
                return true;
            }

            if (!componentInfo.getter)
            {
                ENGINE_PRINT(
                    EngineLogging::LogLevel::Warn,
                    "[ScriptSystem] No getter function for '",
                    std::string(compName),
                    "'");
                lua_pushnil(L);
                return true;
            }

            // Call the getter
            void* compPtr = componentInfo.getter(m_ecs, static_cast<Entity>(entityId));
            //ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] Getter returned ptr=", compPtr, " for comp=", compName, " entity=", entityId);

            if (!compPtr)
            {
                // Optional component queries legitimately return nil. Keeping
                // this at warning level can flood Android logcat from hot Lua
                // paths and turn a cheap miss into formatting + I/O.
                ENGINE_LOG_DEBUG(
                    "[ScriptSystem] Component '" + std::string(compName) +
                    "' not found on entity " + std::to_string(entityId));
                lua_pushnil(L);
                return true;
            }

            // [NEW] SPECIAL CASE: ANIMATION PROXY
            if (compName == "AnimationComponent")
            {
                // Create the proxy on the stack
                LuaAnimationComponent proxy(static_cast<Entity>(entityId));
                (void)luabridge::push(L, proxy);
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
            const std::string componentName(compName);
            ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] Pushing component proxy for ", componentName);
            PushComponentProxy(L, m_ecs, static_cast<Entity>(entityId), componentName);
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
    // If there are script entities that are pending destruction, destroy them first.
    while (!entitiesPendingDestroy.empty()) {
        Entity entityToDestroy = entitiesPendingDestroy.front();
        entitiesPendingDestroy.pop();

        m_ecs->DestroyEntity(entityToDestroy);
    }

    // one-shot reconcile on first update after initialise/play
    if (m_needsReconcile && m_ecs)
    {
        m_needsReconcile = false;
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "[ScriptSystem] One-shot reconcile: reloading all script instances");
        ReloadAllInstances();
    }

    // Advance coroutines and capture the resulting VM once for all script
    // updates. ScriptingRuntime::GetLuaState takes a mutex, so querying it per
    // script is needlessly expensive on script-heavy scenes.
    // Use scaled delta time so coroutines respect pause state
    Scripting::Tick(static_cast<float>(TimeManager::GetDeltaTime()));
    lua_State* const luaState = Scripting::GetLuaState();
    if (luaState) {
        // Heap size in KB; a sawtooth in the profiler report is the collector.
        PROFILE_COUNT("Lua::HeapKB", lua_gc(luaState, LUA_GCCOUNT, 0));
    }
    LuaHeapReport::Tick(luaState);

    // Single lock for entire Update — m_runtimeMap is only accessed from main thread
    // (SequentialSystemOrchestrator::Update is single-threaded)
    std::lock_guard<std::recursive_mutex> lk(m_mutex);

    // Phase 1: Create instances for all entities that need them (no Awake/Start yet)
    m_newlyCreatedEntitiesScratch.clear();
    m_newlyCreatedEntitiesScratch.reserve(entities.size());
    for (Entity e : entities)
    {
        // ScriptSystem membership guarantees ScriptComponentData.
        ScriptComponentData& comp =
            m_ecs->GetComponent<ScriptComponentData>(e);

        // Runtime flags are kept in sync when instances are created, destroyed,
        // reloaded, and shut down. Use them for the common all-created case so
        // every script entity does not probe the runtime hash map each frame.
        bool needsCreation = false;
        for (const ScriptData& script : comp.scripts) {
            if (script.enabled && !script.scriptPath.empty() &&
                !script.instanceCreated) {
                needsCreation = true;
                break;
            }
        }

        if (!needsCreation || !m_ecs->IsEntityActiveInHierarchy(e)) {
            continue;
        }

        if (EnsureInstanceForEntityNoLifecycle(e, *m_ecs)) {
            m_newlyCreatedEntitiesScratch.push_back(e);
        }
    }

    // Phase 2: Call Awake on all newly created instances
    for (Entity e : m_newlyCreatedEntitiesScratch)
    {
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
    for (Entity e : m_newlyCreatedEntitiesScratch)
    {
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
    // Two-pass approach for justActivated: set ALL flags first so any script can
    // read justActivated on ANY entity during Update (cross-entity reads).
    // Previously, the flag was set and cleared per-entity, making it invisible
    // to scripts on other entities (e.g. PauseMenuButtonHandler reading
    // PauseMenuUI.justActivated).
    float dt = static_cast<float>(TimeManager::GetDeltaTime());

    // Pass 4a: detect inactive→active transitions, set justActivated flags
    EntityQueryWrappers::UpdateCacheTiming(dt);
    m_justActivatedEntitiesScratch.clear();
    m_justActivatedEntitiesScratch.reserve(entities.size());
    m_activeUpdateInstancesScratch.clear();
    for (Entity e : entities)
    {
        bool isActive = m_ecs->IsEntityActiveInHierarchy(e);

        if (!isActive) {
            m_prevActiveEntities.reset(e);
            continue;
        }

        if (!m_prevActiveEntities.test(e)) {
            m_prevActiveEntities.set(e);
            auto activeComponent = m_ecs->TryGetComponent<ActiveComponent>(e);
            if (activeComponent) {
                activeComponent->get().justActivated = true;
                m_justActivatedEntitiesScratch.push_back(e);
            }
        }

        auto runtimeIt = m_runtimeMap.find(e);
        if (runtimeIt == m_runtimeMap.end()) {
            continue;
        }
        for (auto& scriptInst : runtimeIt->second) {
            if (scriptInst && scriptInst->HasUpdate()) {
                m_activeUpdateInstancesScratch.push_back(scriptInst.get());
            }
        }
    }

    // Pass 4b: run script Updates (all justActivated flags are visible)
#if defined(TRACY_ENABLE)
    for (Scripting::ScriptComponent* scriptInst : m_activeUpdateInstancesScratch)
    {
        {
            PROFILE_FUNCTION();
            const auto& path = scriptInst->GetScriptPath();
            ZoneName(path.c_str(), path.size());
            scriptInst->Update(dt, luaState);
        }
    }
#else
    Scripting::ScriptComponent::UpdateBatch(
        dt, luaState, m_activeUpdateInstancesScratch.data(), m_activeUpdateInstancesScratch.size());
#endif

    // Pass 4c: clear only flags raised above rather than scanning every script entity.
    for (Entity e : m_justActivatedEntitiesScratch)
    {
        auto activeComponent = m_ecs->TryGetComponent<ActiveComponent>(e);
        if (activeComponent) {
            activeComponent->get().justActivated = false;
        }
    }

    // Membership changes are rare. Avoid scanning the runtime hash map and
    // probing the system's tree for every script entity on unchanged frames.
    const std::uint64_t membershipVersion = entities.Version();
    if (m_lastMembershipVersion != membershipVersion)
    {
        m_runtimeRemovalScratch.clear();
        for (auto& p : m_runtimeMap)
        {
            Entity e = p.first;
            if (!entities.contains(e)) m_runtimeRemovalScratch.push_back(e);
        }
        for (Entity e : m_runtimeRemovalScratch)
        {
            DestroyInstanceForEntity(e);
        }
        m_lastMembershipVersion = membershipVersion;
    }
}

void ScriptSystem::Shutdown()
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);

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
    for (auto& eventMask : m_intEventMasks) {
        eventMask.store(0, std::memory_order_relaxed);
    }
    m_prevActiveEntities.reset();
    m_justActivatedEntitiesScratch.clear();
    m_activeUpdateInstancesScratch.clear();
    m_lastMembershipVersion = 0;

    g_ecsManager = nullptr;
    g_entityByNameCache.clear();
    EntityQueryWrappers::ClearEnemyCaches();
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
            auto& scriptVec = m_runtimeMap[e];

            // Resize vector if needed
            if (scriptIdx >= scriptVec.size())
            {
                scriptVec.resize(scriptIdx + 1);
            }

            scriptVec[scriptIdx] = std::move(runtimeComp);
            script.instanceId = scPtr ? scPtr->GetInstanceRef() : LUA_NOREF;
            script.instanceCreated = (scPtr != nullptr);
            RefreshEntityIntEventMask(e);
            // Notify listeners that instances for 'e' have been created/changed.
            NotifyInstancesChanged(e);
        }

        // Call lifecycle methods
        if (scPtr)
        {
            scPtr->Awake();
            scPtr->Start();

            if (script.autoInvokeEntry && !script.entryFunction.empty()) {
                Scripting::CallInstanceFunction(script.instanceId, script.entryFunction);
            }
        }
    }

    RefreshEntityIntEventMask(e);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
            auto& scriptVec = m_runtimeMap[e];

            // Resize vector if needed
            if (scriptIdx >= scriptVec.size())
            {
                scriptVec.resize(scriptIdx + 1);
            }

            scriptVec[scriptIdx] = std::move(runtimeComp);
            script.instanceId = scPtr ? scPtr->GetInstanceRef() : LUA_NOREF;
            script.instanceCreated = (scPtr != nullptr);
            RefreshEntityIntEventMask(e);
            // Notify listeners that instances for 'e' have been created/changed.
            NotifyInstancesChanged(e);
        }

        anyCreated = true;
        // NOTE: Awake/Start are NOT called here - caller is responsible for calling them
    }

    RefreshEntityIntEventMask(e);
    return anyCreated;
}

void ScriptSystem::DestroyInstanceForEntity(Entity e)
{
    if (e < MAX_ENTITIES) {
        m_prevActiveEntities.reset(e);
        m_intEventMasks[e].store(0, std::memory_order_relaxed);
    }

    std::vector<std::unique_ptr<Scripting::ScriptComponent>> runtimePtrs;
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
            m_intEventMasks[e].store(0, std::memory_order_relaxed);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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

bool ScriptSystem::CallEntityFunctionWithInt(Entity e, const std::string& funcName, int intArg, ECSManager& ecsManager)
{
    if (!CanEntityHandleIntEvent(e, funcName)) return false;

    ScriptComponentData* comp = GetScriptComponent(e, ecsManager);
    if (!comp) return false;

    if (!Scripting::GetLuaState()) return false;

    // Collision events are dispatched frequently, especially Stay. Avoid the
    // full reconciliation path when every enabled script already has a runtime.
    bool needsReconciliation = false;
    for (const ScriptData& script : comp->scripts) {
        if (script.enabled && !script.scriptPath.empty() &&
            !script.instanceCreated) {
            needsReconciliation = true;
            break;
        }
    }

    if (needsReconciliation && !EnsureInstanceForEntity(e, ecsManager)) return false;

    bool anySuccess = false;
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        auto it = m_runtimeMap.find(e);
        if (it != m_runtimeMap.end())
        {
            for (auto& scriptInst : it->second)
            {
                if (scriptInst && scriptInst->CanHandleIntEvent(funcName))
                {
                    int instRef = scriptInst->GetInstanceRef();
                    if (Scripting::CallInstanceFunctionWithInt(instRef, funcName, intArg))
                    {
                        anySuccess = true;
                    }
                }
            }
        }
    }

    return anySuccess;
}

bool ScriptSystem::CanEntityHandleIntEvent(Entity e, std::string_view funcName) const noexcept
{
    const std::uint8_t eventBit = GetTrackedIntEventBit(funcName);
    if (eventBit == 0) {
        return true;
    }
    if (e >= MAX_ENTITIES) {
        return false;
    }
    const std::uint8_t eventState =
        m_intEventMasks[e].load(std::memory_order_relaxed);
    if ((eventState & kIntEventMaskKnown) == 0) {
        // The runtime has not reconciled this entity yet. Preserve the old
        // behavior and let the call path create its instance on demand.
        return true;
    }
    return (eventState & eventBit) != 0;
}

void ScriptSystem::RefreshEntityIntEventMask(Entity e)
{
    if (e >= MAX_ENTITIES) return;

    std::uint8_t aggregateMask = 0;
    auto runtimeIt = m_runtimeMap.find(e);
    if (runtimeIt != m_runtimeMap.end()) {
        for (const auto& script : runtimeIt->second) {
            if (script) {
                aggregateMask |= script->GetIntEventMask();
            }
        }
    }
    m_intEventMasks[e].store(
        static_cast<std::uint8_t>(aggregateMask | kIntEventMaskKnown),
        std::memory_order_relaxed);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        const auto& denseEntities = entities.DenseView();
        entitySnapshot.assign(denseEntities.begin(), denseEntities.end());
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
            std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
                m_intEventMasks[e].store(0, std::memory_order_relaxed);
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

        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
    auto component = ecsManager.TryGetComponent<ScriptComponentData>(e);
    return component ? &component->get() : nullptr;
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
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_instancesChangedCbs.emplace_back(key, std::move(cb));
}

void ScriptSystem::UnregisterInstancesChangedCallback(void* cbId)
{
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
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
    //    std::lock_guard<std::recursive_mutex> lk(m_mutex);
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

    std::lock_guard<std::recursive_mutex> lk(m_mutex);

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

void ScriptSystem::AddEntityPendingDestroy(Entity entity) {
    entitiesPendingDestroy.push(entity);
}

void ScriptSystem::ClearEntitiesPendingDestroy() {
    while (!entitiesPendingDestroy.empty()) {
        entitiesPendingDestroy.pop();
    }
}
