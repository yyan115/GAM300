#include "pch.h"
#include "UI/Slider/SliderSystem.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "UI/Slider/SliderComponent.hpp"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/NameComponent.hpp"
#include "Logging.hpp"
#include "Input/InputManager.h"
#include "Graphics/GraphicsManager.hpp"
#include "WindowManager.hpp"
#include "Transform/TransformComponent.hpp"
#include "Transform/TransformSystem.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "Hierarchy/ChildrenComponent.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Utilities/GUID.hpp"
#include "Script/ScriptSystem.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Graphics/ShaderClass.h"
#include <cmath>

namespace {
    Entity InvalidEntity() {
        return static_cast<Entity>(-1);
    }

    float Clamp01(float v) {
        return std::max(0.0f, std::min(1.0f, v));
    }
}

void SliderSystem::Initialise(ECSManager& ecsManager) {
    m_ecs = &ecsManager;
}

void SliderSystem::Shutdown() {
    m_ecs = nullptr;
    m_activeSlider = InvalidEntity();
    ENGINE_PRINT("[SliderSystem] Shutdown complete");
}

Vector3D SliderSystem::GetMousePosInGameSpace() const {
    if (!g_inputManager) {
        return Vector3D(0.0f, 0.0f, 0.0f);
    }

    // Get target game resolution (world coordinate space for 2D UI)
    int gameResWidth, gameResHeight;
    GraphicsManager::GetInstance().GetTargetGameResolution(gameResWidth, gameResHeight);

    // Get pointer position in normalized coordinates (0-1)
    glm::vec2 pointerPos = g_inputManager->GetPointerPosition();

    // Convert from normalized (0-1, top-left origin) to game space coordinates
    float gameX = pointerPos.x * static_cast<float>(gameResWidth);
    float gameY = (1.0f - pointerPos.y) * static_cast<float>(gameResHeight);  // Flip Y

    return Vector3D(gameX, gameY, 0.0f);
}

bool SliderSystem::TryResolveEntity(const GUID_128& guid, Entity& outEntity) const {
    if (guid.high == 0 && guid.low == 0) {
        return false;
    }
    outEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(guid);
    return outEntity != InvalidEntity();
}

void SliderSystem::AttachChild(Entity parent, Entity child) {
    if (!m_ecs) return;

    auto& guidRegistry = EntityGUIDRegistry::GetInstance();
    GUID_128 parentGuid = guidRegistry.GetGUIDByEntity(parent);
    GUID_128 childGuid = guidRegistry.GetGUIDByEntity(child);

    if (!m_ecs->HasComponent<ParentComponent>(child)) {
        m_ecs->AddComponent<ParentComponent>(child, ParentComponent{ parentGuid });
    } else {
        m_ecs->GetComponent<ParentComponent>(child).parent = parentGuid;
    }

    if (!m_ecs->HasComponent<ChildrenComponent>(parent)) {
        m_ecs->AddComponent<ChildrenComponent>(parent, ChildrenComponent{});
    }

    auto& children = m_ecs->GetComponent<ChildrenComponent>(parent).children;
    if (std::find(children.begin(), children.end(), childGuid) == children.end()) {
        children.push_back(childGuid);
    }
}

void SliderSystem::EnsureChildEntities(Entity sliderEntity, SliderComponent& sliderComp) {
    if (!m_ecs) return;

    auto& guidRegistry = EntityGUIDRegistry::GetInstance();

    auto removeChildGuid = [&](const GUID_128& guid) {
        if (!m_ecs->HasComponent<ChildrenComponent>(sliderEntity)) return;
        auto& children = m_ecs->GetComponent<ChildrenComponent>(sliderEntity).children;
        children.erase(std::remove(children.begin(), children.end(), guid), children.end());
        if (children.empty()) {
            m_ecs->RemoveComponent<ChildrenComponent>(sliderEntity);
        }
    };

    Entity trackEntity = InvalidEntity();
    if (!TryResolveEntity(sliderComp.trackEntityGuid, trackEntity)) {
        if (sliderComp.trackEntityGuid.high != 0 || sliderComp.trackEntityGuid.low != 0) {
            removeChildGuid(sliderComp.trackEntityGuid);
        }

        trackEntity = m_ecs->CreateEntity();
        sliderComp.trackEntityGuid = guidRegistry.GetGUIDByEntity(trackEntity);

        auto& name = m_ecs->GetComponent<NameComponent>(trackEntity).name;
        name = "Slider_Track";

        // Add SpriteRenderComponent with default sprite shader
        SpriteRenderComponent sprite;
        GUID_128 spriteShaderGuid = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));
        sprite.shaderGUID = spriteShaderGuid;
        sprite.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("sprite"));
        sprite.is3D = false;
        sprite.isVisible = true;
        sprite.sortingOrder = 0;
        m_ecs->AddComponent<SpriteRenderComponent>(trackEntity, sprite);

        // Set default scale to match parent
        if (m_ecs->HasComponent<Transform>(sliderEntity)) {
            auto& parentTransform = m_ecs->GetComponent<Transform>(sliderEntity);
            auto& trackTransform = m_ecs->GetComponent<Transform>(trackEntity);
            trackTransform.localScale = parentTransform.localScale;
            trackTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
            trackTransform.isDirty = true;
        }

        if (m_ecs->transformSystem) {
            m_ecs->transformSystem->UpdateTransform(trackEntity);
        }
    }

    Entity handleEntity = InvalidEntity();
    if (!TryResolveEntity(sliderComp.handleEntityGuid, handleEntity)) {
        if (sliderComp.handleEntityGuid.high != 0 || sliderComp.handleEntityGuid.low != 0) {
            removeChildGuid(sliderComp.handleEntityGuid);
        }

        handleEntity = m_ecs->CreateEntity();
        sliderComp.handleEntityGuid = guidRegistry.GetGUIDByEntity(handleEntity);

        auto& name = m_ecs->GetComponent<NameComponent>(handleEntity).name;
        name = "Slider_Handle";

        SpriteRenderComponent sprite;
        GUID_128 spriteShaderGuid = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));
        sprite.shaderGUID = spriteShaderGuid;
        sprite.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("sprite"));
        sprite.is3D = false;
        sprite.isVisible = true;
        sprite.sortingOrder = 1;
        m_ecs->AddComponent<SpriteRenderComponent>(handleEntity, sprite);

        // Default handle scale based on parent height
        if (m_ecs->HasComponent<Transform>(sliderEntity)) {
            auto& parentTransform = m_ecs->GetComponent<Transform>(sliderEntity);
            auto& handleTransform = m_ecs->GetComponent<Transform>(handleEntity);
            float size = std::max(10.0f, parentTransform.localScale.y);
            handleTransform.localScale = Vector3D(size, size, 1.0f);
            handleTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
            handleTransform.isDirty = true;
        }

        if (m_ecs->transformSystem) {
            m_ecs->transformSystem->UpdateTransform(handleEntity);
        }
    }

    if (trackEntity != InvalidEntity()) {
        AttachChild(sliderEntity, trackEntity);
    }
    if (handleEntity != InvalidEntity()) {
        AttachChild(sliderEntity, handleEntity);
    }
}

bool SliderSystem::IsMouseOverEntity(Entity entity, const Vector3D& mousePos) const {
    if (!m_ecs) return false;
    if (!m_ecs->HasComponent<Transform>(entity)) return false;

    auto& transform = m_ecs->GetComponent<Transform>(entity);

    // Extract world position and scale from world matrix
    Vector3D worldPos = Matrix4x4::ExtractTranslation(transform.worldMatrix);
    Vector3D worldScale = Matrix4x4::ExtractScale(transform.worldMatrix);

    float halfExtentsX = worldScale.x / 2.0f;
    float halfExtentsY = worldScale.y / 2.0f;
    float minX = worldPos.x - halfExtentsX;
    float maxX = worldPos.x + halfExtentsX;
    float minY = worldPos.y - halfExtentsY;
    float maxY = worldPos.y + halfExtentsY;

    return (mousePos.x >= minX && mousePos.x <= maxX && mousePos.y >= minY && mousePos.y <= maxY);
}

bool SliderSystem::UpdateValueFromMouse(Entity sliderEntity, SliderComponent& sliderComp, const Vector3D& mousePos) {
    if (!m_ecs) return false;

    Entity trackEntity = InvalidEntity();
    Entity handleEntity = InvalidEntity();
    if (!TryResolveEntity(sliderComp.trackEntityGuid, trackEntity)) return false;
    if (!TryResolveEntity(sliderComp.handleEntityGuid, handleEntity)) return false;

    if (!m_ecs->HasComponent<Transform>(trackEntity) || !m_ecs->HasComponent<Transform>(handleEntity)) return false;
    if (!m_ecs->HasComponent<Transform>(sliderEntity)) return false;

    auto& trackTransform = m_ecs->GetComponent<Transform>(trackEntity);
    auto& handleTransform = m_ecs->GetComponent<Transform>(handleEntity);
    auto& parentTransform = m_ecs->GetComponent<Transform>(sliderEntity);

    Vector3D trackWorldPos = parentTransform.worldMatrix.TransformPoint(trackTransform.localPosition);
    Vector3D trackWorldScale = Matrix4x4::ExtractScale(trackTransform.worldMatrix);
    Vector3D handleWorldScale = Matrix4x4::ExtractScale(handleTransform.worldMatrix);

    float trackLength = sliderComp.horizontal ? trackWorldScale.x : trackWorldScale.y;
    float handleLength = sliderComp.horizontal ? handleWorldScale.x : handleWorldScale.y;

    float range = std::max(0.0f, trackLength - handleLength);
    if (range <= 0.0f) return false;

    float mouseRelative = sliderComp.horizontal
        ? (mousePos.x - (trackWorldPos.x - trackLength / 2.0f + handleLength / 2.0f))
        : (mousePos.y - (trackWorldPos.y - trackLength / 2.0f + handleLength / 2.0f));

    float t = Clamp01(mouseRelative / range);
    float newValue = sliderComp.minValue + t * (sliderComp.maxValue - sliderComp.minValue);

    if (sliderComp.wholeNumbers) {
        newValue = std::round(newValue);
    }

    newValue = std::max(sliderComp.minValue, std::min(sliderComp.maxValue, newValue));

    if (newValue != sliderComp.value) {
        sliderComp.value = newValue;
        return true;
    }

    return false;
}

void SliderSystem::UpdateHandleFromValue(Entity sliderEntity, SliderComponent& sliderComp) {
    if (!m_ecs) return;

    Entity trackEntity = InvalidEntity();
    Entity handleEntity = InvalidEntity();
    if (!TryResolveEntity(sliderComp.trackEntityGuid, trackEntity)) return;
    if (!TryResolveEntity(sliderComp.handleEntityGuid, handleEntity)) return;

    if (!m_ecs->HasComponent<Transform>(trackEntity) || !m_ecs->HasComponent<Transform>(handleEntity)) return;

    auto& trackTransform = m_ecs->GetComponent<Transform>(trackEntity);
    auto& handleTransform = m_ecs->GetComponent<Transform>(handleEntity);

    float trackLength = sliderComp.horizontal ? trackTransform.localScale.x : trackTransform.localScale.y;
    float handleLength = sliderComp.horizontal ? handleTransform.localScale.x : handleTransform.localScale.y;
    float range = std::max(0.0f, trackLength - handleLength);

    float t = 0.0f;
    if (sliderComp.maxValue != sliderComp.minValue) {
        t = (sliderComp.value - sliderComp.minValue) / (sliderComp.maxValue - sliderComp.minValue);
    }
    t = Clamp01(t);

    float offset = (-range / 2.0f) + t * range;
    if (sliderComp.horizontal) {
        handleTransform.localPosition.x = offset;
        handleTransform.localPosition.y = 0.0f;
    } else {
        handleTransform.localPosition.y = offset;
        handleTransform.localPosition.x = 0.0f;
    }

    handleTransform.isDirty = true;

    if (m_ecs->transformSystem) {
        m_ecs->transformSystem->UpdateTransform(handleEntity);
    }
}

void SliderSystem::InvokeOnValueChanged(Entity sliderEntity, SliderComponent& sliderComp, float oldValue) {
    if (!m_ecs) return;
    if (oldValue == sliderComp.value) return;

    auto* scriptSystem = m_ecs->scriptSystem.get();
    if (!scriptSystem) return;

    for (const auto& binding : sliderComp.onValueChanged) {
        if (binding.scriptGuidStr.empty() || binding.functionName.empty()) {
            continue;
        }

        Entity targetEntity = sliderEntity;
        if (!binding.targetEntityGuidStr.empty()) {
            GUID_128 targetGuid = GUIDUtilities::ConvertStringToGUID128(binding.targetEntityGuidStr);
            Entity resolved = EntityGUIDRegistry::GetInstance().GetEntityByGUID(targetGuid);
            if (resolved != InvalidEntity()) {
                targetEntity = resolved;
            }
        }

        bool callSucceeded = false;

        callSucceeded = scriptSystem->CallInstanceFunctionByScriptGuid(
            targetEntity,
            binding.scriptGuidStr,
            binding.functionName
        );

        if (!callSucceeded && !binding.scriptPath.empty()) {
            callSucceeded = scriptSystem->CallStandaloneScriptFunctionWithEntity(
                binding.scriptPath,
                binding.scriptGuidStr,
                binding.functionName,
                targetEntity
            );
        }

        if (!callSucceeded) {
            callSucceeded = scriptSystem->CallEntityFunction(
                targetEntity,
                binding.functionName,
                *m_ecs
            );
        }

        if (!callSucceeded) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn,
                "[SliderSystem] Failed to invoke callback: target=",
                binding.targetEntityGuidStr,
                " script=", binding.scriptGuidStr,
                " path=", binding.scriptPath,
                " fn=", binding.functionName);
        }
    }
}

void SliderSystem::Update() {
    PROFILE_FUNCTION();
    if (!m_ecs || !g_inputManager) return;

    const bool mousePressed = g_inputManager->IsPointerJustPressed();
    const bool mouseHeld = g_inputManager->IsPointerPressed();

    if (!mouseHeld) {
        m_activeSlider = InvalidEntity();
    }

    Vector3D mousePos = GetMousePosInGameSpace();

    for (Entity sliderEntity : entities) {
        if (!m_ecs->HasComponent<SliderComponent>(sliderEntity)) continue;

        auto& sliderComp = m_ecs->GetComponent<SliderComponent>(sliderEntity);
        EnsureChildEntities(sliderEntity, sliderComp);

        // Clamp and normalize value
        if (sliderComp.wholeNumbers) {
            sliderComp.value = std::round(sliderComp.value);
        }
        sliderComp.value = std::max(sliderComp.minValue, std::min(sliderComp.maxValue, sliderComp.value));

        float oldValue = sliderComp.value;

        if (sliderComp.interactable) {
            if (mousePressed && m_activeSlider == InvalidEntity()) {
                // Click on handle starts drag
                Entity handleEntity = InvalidEntity();
                if (TryResolveEntity(sliderComp.handleEntityGuid, handleEntity) && IsMouseOverEntity(handleEntity, mousePos)) {
                    m_activeSlider = sliderEntity;
                }
                // Clicking track sets value and activates slider
                else {
                    Entity trackEntity = InvalidEntity();
                    if (TryResolveEntity(sliderComp.trackEntityGuid, trackEntity) && IsMouseOverEntity(trackEntity, mousePos)) {
                        m_activeSlider = sliderEntity;
                        UpdateValueFromMouse(sliderEntity, sliderComp, mousePos);
                    }
                }
            }

            if (mouseHeld && m_activeSlider == sliderEntity) {
                UpdateValueFromMouse(sliderEntity, sliderComp, mousePos);
            }
        }

        UpdateHandleFromValue(sliderEntity, sliderComp);

        if (oldValue != sliderComp.value) {
            InvokeOnValueChanged(sliderEntity, sliderComp, oldValue);
            
            // Re-fetch component reference after callback in case ECS storage was reallocated
            if (!m_ecs->HasComponent<SliderComponent>(sliderEntity)) continue;
            auto& sliderCompAfterCallback = m_ecs->GetComponent<SliderComponent>(sliderEntity);
            sliderCompAfterCallback.lastValue = sliderCompAfterCallback.value;
        } else {
            sliderComp.lastValue = sliderComp.value;
        }
    }
}
