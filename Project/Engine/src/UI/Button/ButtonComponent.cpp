#include <pch.h>
#include "UI/Button/ButtonComponent.hpp"
#include "Logging.hpp"
#include "ECS/ECSRegistry.hpp"

#pragma region Reflection

REFL_REGISTER_START(ButtonBinding)
REFL_REGISTER_PROPERTY(targetEntityGuidStr)
REFL_REGISTER_PROPERTY(scriptPath)
REFL_REGISTER_PROPERTY(scriptGuidStr)
REFL_REGISTER_PROPERTY(functionName)
REFL_REGISTER_PROPERTY(callWithSelf)
REFL_REGISTER_END

REFL_REGISTER_START(ButtonComponent)
REFL_REGISTER_PROPERTY(bindings)
REFL_REGISTER_PROPERTY(interactable)
REFL_REGISTER_END

#pragma endregion

ButtonController::ButtonController(Entity owner)
    : m_entity(owner)
{
    // Initialize cache size from serialized ButtonComponent if available
    ECSManager* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs) return;

    auto* btnData = ecs->HasComponent<ButtonComponent>(m_entity)
        ? &ecs->GetComponent<ButtonComponent>(m_entity)
        : nullptr;

    size_t n = btnData ? btnData->bindings.size() : 0;
    m_cachedInstanceRef.assign(n, LUA_NOREF);
}

ButtonController::~ButtonController()
{
    // Unregister callback - ScriptSystem handles thread-safety
    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs && ecs->scriptSystem && m_instancesCbId) {
        ecs->scriptSystem->UnregisterInstancesChangedCallback(m_instancesCbId);
        m_instancesCbId = nullptr;
    }

    // Simple clear - no mutex needed
    m_cachedInstanceRef.clear();
}

void ButtonController::SetEntity(Entity owner)
{
    m_entity = owner;

    // Initialize cache size
    ECSManager* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs) return;

    auto* btnData = ecs->HasComponent<ButtonComponent>(m_entity)
        ? &ecs->GetComponent<ButtonComponent>(m_entity)
        : nullptr;

    size_t n = btnData ? btnData->bindings.size() : 0;
    m_cachedInstanceRef.assign(n, LUA_NOREF);
}

void ButtonController::OnEnable()
{
    if (m_entity == 0) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonController] OnEnable called with invalid entity");
        return;
    }

    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs || !ecs->scriptSystem) return;

    // Register callback
    std::function<void(Entity)> cb = [this](Entity e) {
        this->InstancesChangedCallback(e);
        };

    void* key = reinterpret_cast<void*>(cb.target<void()>());
    m_instancesCbId = key;

    ecs->scriptSystem->RegisterInstancesChangedCallback(cb);
}

void ButtonController::OnDisable()
{
    // Unregister callback
    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs && ecs->scriptSystem && m_instancesCbId) {
        ecs->scriptSystem->UnregisterInstancesChangedCallback(m_instancesCbId);
        m_instancesCbId = nullptr;
    }

    // Clear cache
    m_cachedInstanceRef.clear();
}

void ButtonController::InstancesChangedCallback(Entity e)
{
    // No mutex needed - simple invalidation
    // Worst case: we read stale cache during invalidation and get a cache miss
    for (auto& r : m_cachedInstanceRef) {
        r = LUA_NOREF;
    }
}

void ButtonController::OnClick()
{
    if (m_entity == 0) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonController] OnClick called with invalid entity");
        return;
    }

    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs) return;

    if (!ecs->HasComponent<ButtonComponent>(m_entity)) return;

    const auto& bc = ecs->GetComponent<ButtonComponent>(m_entity);
    if (!bc.interactable) return;

    auto* scriptSystem = ecs->scriptSystem.get();
    if (!scriptSystem) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonController] ScriptSystem not available");
        return;
    }

    // Iterate bindings and call each
    for (size_t i = 0; i < bc.bindings.size(); ++i) {
        const ButtonBinding& binding = bc.bindings[i];

        if (binding.scriptGuidStr.empty() || binding.functionName.empty()) {
            continue;
        }

        // Determine target entity
        Entity targetEntity = m_entity;
        if (!binding.targetEntityGuidStr.empty()) {
            // TODO: Resolve targetEntityGuidStr to Entity
        }

        bool callSucceeded = false;

        // Method 1: Try calling via entity's ScriptComponent (if it has the script attached)
        int cachedRef = LUA_NOREF;
        if (i < m_cachedInstanceRef.size()) {
            cachedRef = m_cachedInstanceRef[i];
        }

        if (cachedRef != LUA_NOREF) {
            callSucceeded = scriptSystem->CallInstanceFunctionByScriptGuid(
                targetEntity,
                binding.scriptGuidStr,
                binding.functionName
            );

            if (!callSucceeded) {
                if (i < m_cachedInstanceRef.size()) {
                    m_cachedInstanceRef[i] = LUA_NOREF;
                }
            }
        }

        if (!callSucceeded) {
            callSucceeded = scriptSystem->CallInstanceFunctionByScriptGuid(
                targetEntity,
                binding.scriptGuidStr,
                binding.functionName
            );

            if (callSucceeded) {
                int resolvedRef = scriptSystem->GetInstanceRefForScript(
                    targetEntity,
                    binding.scriptGuidStr
                );

                if (resolvedRef != LUA_NOREF) {
                    if (i >= m_cachedInstanceRef.size()) {
                        m_cachedInstanceRef.resize(i + 1, LUA_NOREF);
                    }
                    m_cachedInstanceRef[i] = resolvedRef;
                }
            }
        }

        // Method 2: Try standalone script instance (ButtonComponent doesn't need ScriptComponent)
        // This creates a script instance just for the button callback
        if (!callSucceeded && !binding.scriptPath.empty()) {
            callSucceeded = scriptSystem->CallStandaloneScriptFunction(
                binding.scriptPath,
                binding.scriptGuidStr,
                binding.functionName
            );
        }

        // Method 3: Fallback - try calling on any script attached to entity that has this function
        if (!callSucceeded) {
            callSucceeded = scriptSystem->CallEntityFunction(
                targetEntity,
                binding.functionName,
                *ecs
            );
        }

        if (!callSucceeded) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn,
                "[ButtonController] Failed to invoke callback: target=",
                binding.targetEntityGuidStr,
                " script=", binding.scriptGuidStr,
                " path=", binding.scriptPath,
                " fn=", binding.functionName);
        }
        else {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug,
                "[ButtonController] Successfully called ", binding.functionName,
                " on script ", binding.scriptGuidStr);
        }
    }
}