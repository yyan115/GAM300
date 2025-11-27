// ButtonComponent.cpp
#include <pch.h>
#include "UI/Button/ButtonComponent.hpp"
#include "Logging.hpp"
#include "ECS/ECSRegistry.hpp"

#pragma region Reflection

// Register ButtonBinding
REFL_REGISTER_START(ButtonBinding)
    REFL_REGISTER_PROPERTY(targetEntityGuidStr)
    REFL_REGISTER_PROPERTY(scriptGuidStr)
    REFL_REGISTER_PROPERTY(functionName)
    REFL_REGISTER_PROPERTY(callWithSelf)
REFL_REGISTER_END

// Register ButtonComponentData
REFL_REGISTER_START(ButtonComponentData)
    REFL_REGISTER_PROPERTY(bindings)
    REFL_REGISTER_PROPERTY(interactable)
REFL_REGISTER_END

#pragma endregion

ButtonComponent::ButtonComponent(Entity owner)
    : m_entity(owner)
{
    // Initialize cache size from serialized ButtonComponentData if available
    ECSManager* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs) return;

    auto* btnData = ecs->HasComponent<ButtonComponentData>(m_entity)
        ? &ecs->GetComponent<ButtonComponentData>(m_entity)
        : nullptr;

    size_t n = btnData ? btnData->bindings.size() : 0;
    m_cachedInstanceRef.assign(n, LUA_NOREF);
}
ButtonComponent::~ButtonComponent()
{
    // CRITICAL: Unregister callback FIRST before any mutex operations
    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs && ecs->scriptSystem && m_instancesCbId) {
        ecs->scriptSystem->UnregisterInstancesChangedCallback(m_instancesCbId);
        m_instancesCbId = nullptr;
    }

    // NOW safe to clear cache - no more callbacks can arrive
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        m_cachedInstanceRef.clear();
    }
}

void ButtonComponent::OnEnable()
{
    // Register instances-changed callback to invalidate cache when ScriptSystem recreates instances
    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs || !ecs->scriptSystem) return;

    // Create a std::function (not just a lambda) so we can use .target<void>()
    std::function<void(Entity)> cb = [this](Entity e) { this->InstancesChangedCallback(e); };

    // Use cb.target<void>() as key for unregistering
    void* key = reinterpret_cast<void*>(cb.target<void()>());
    m_instancesCbId = key;

    ecs->scriptSystem->RegisterInstancesChangedCallback(cb);
}

void ButtonComponent::OnDisable()
{
    // Unregister callback FIRST
    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (ecs && ecs->scriptSystem && m_instancesCbId) {
        ecs->scriptSystem->UnregisterInstancesChangedCallback(m_instancesCbId);
        m_instancesCbId = nullptr;
    }

    // Then clear cache
    std::lock_guard<std::mutex> lk(m_cacheMutex);
    m_cachedInstanceRef.clear();
}

void ButtonComponent::InstancesChangedCallback(Entity e)
{
    // Add safety check - try to lock with timeout
    std::unique_lock<std::mutex> lk(m_cacheMutex, std::defer_lock);

    if (!lk.try_lock()) {
        // Mutex is locked elsewhere (possibly being destroyed), bail out
        ENGINE_PRINT(EngineLogging::LogLevel::Debug,
            "[ButtonComponent] Could not acquire lock in callback - object may be destroying");
        return;
    }

    // Safe to invalidate cache
    for (auto& r : m_cachedInstanceRef) {
        r = LUA_NOREF;
    }
}


void ButtonComponent::OnClick()
{
    auto* ecs = &ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecs) return;

    if (!ecs->HasComponent<ButtonComponentData>(m_entity)) return;

    const auto& bc = ecs->GetComponent<ButtonComponentData>(m_entity);
    if (!bc.interactable) return;

    auto* scriptSystem = ecs->scriptSystem.get();
    if (!scriptSystem) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "[ButtonComponent] ScriptSystem not available");
        return;
    }

    // Iterate bindings and call each
    for (size_t i = 0; i < bc.bindings.size(); ++i) {
        const ButtonBinding& binding = bc.bindings[i];

        if (binding.scriptGuidStr.empty() || binding.functionName.empty()) {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug,
                "[ButtonComponent] Binding ", i, " has empty script GUID or function name");
            continue;
        }

        // Determine target entity (if targetEntityGuidStr is empty, assume same entity as button)
        Entity targetEntity = m_entity;
        if (!binding.targetEntityGuidStr.empty()) {
            // TODO: Resolve targetEntityGuidStr to Entity
            // For now, assume it's the same entity or implement GUID->Entity lookup
            // Example: targetEntity = ecs->GetEntityByGUID(binding.targetEntityGuid);
        }

        bool callSucceeded = false;

        // Try fast path: use cached instanceRef
        int cachedRef = LUA_NOREF;
        {
            std::lock_guard<std::mutex> lk(m_cacheMutex);
            if (i < m_cachedInstanceRef.size()) {
                cachedRef = m_cachedInstanceRef[i];
            }
        }

        if (cachedRef != LUA_NOREF) {
            // Attempt to call using cached instance ref via ScriptSystem
            callSucceeded = scriptSystem->CallInstanceFunctionByScriptGuid(
                targetEntity,
                binding.scriptGuidStr,
                binding.functionName
            );

            if (!callSucceeded) {
                // Cache is stale, invalidate it
                std::lock_guard<std::mutex> lk(m_cacheMutex);
                if (i < m_cachedInstanceRef.size()) {
                    m_cachedInstanceRef[i] = LUA_NOREF;
                }
            }
        }

        // Slow path: resolve and cache
        if (!callSucceeded) {
            callSucceeded = scriptSystem->CallInstanceFunctionByScriptGuid(
                targetEntity,
                binding.scriptGuidStr,
                binding.functionName
            );

            if (callSucceeded) {
                // Update cache for next time
                int resolvedRef = scriptSystem->GetInstanceRefForScript(
                    targetEntity,
                    binding.scriptGuidStr
                );

                if (resolvedRef != LUA_NOREF) {
                    std::lock_guard<std::mutex> lk(m_cacheMutex);
                    if (i >= m_cachedInstanceRef.size()) {
                        m_cachedInstanceRef.resize(i + 1, LUA_NOREF);
                    }
                    m_cachedInstanceRef[i] = resolvedRef;
                }
            }
        }

        // Final fallback: try calling on any script that has this function
        if (!callSucceeded) {
            callSucceeded = scriptSystem->CallEntityFunction(
                targetEntity,
                binding.functionName,
                *ecs
            );
        }

        if (!callSucceeded) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn,
                "[ButtonComponent] Failed to invoke callback: target=",
                binding.targetEntityGuidStr,
                " script=", binding.scriptGuidStr,
                " fn=", binding.functionName);
        }
        else {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug,
                "[ButtonComponent] Successfully called ", binding.functionName,
                " on script ", binding.scriptGuidStr);
        }
    }
}