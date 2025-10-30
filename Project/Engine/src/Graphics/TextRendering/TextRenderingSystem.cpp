#include "pch.h"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/TextRendering/TextUtils.hpp"
#include "Transform/TransformComponent.hpp"
#include <Asset Manager/AssetManager.hpp>
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ActiveComponent.hpp"

bool TextRenderingSystem::Initialise()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    for (const auto& entity : entities) {
        auto& textComp = ecsManager.GetComponent<TextRenderComponent>(entity);
        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComp.fontGUID);
        textComp.font = ResourceManager::GetInstance().GetFontResourceFromGUID(textComp.fontGUID, fontPath, textComp.fontSize);
#ifndef ANDROID
        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComp.shaderGUID);
        textComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(textComp.shaderGUID, shaderPath);
#else
        std::string shaderPath = ResourceManager::GetPlatformShaderPath("text");
        textComp.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
#endif
    }

    ENGINE_PRINT("[TextSystem] Initialized\n");
	return true;
}

void TextRenderingSystem::Update()
{
    PROFILE_FUNCTION();
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Submit all visible text components to the graphics manager
    for (const auto& entity : entities)
    {
        // Skip inactive entities (Unity-like behavior)
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue; // Don't render inactive entities
            }
        }

        auto& textComponent = ecsManager.GetComponent<TextRenderComponent>(entity);

        // Check if font size changed and reload if needed
        if (textComponent.font && textComponent.font->GetFontSize() != textComponent.fontSize) {
            std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.fontGUID);
            textComponent.font = ResourceManager::GetInstance().GetFontResourceFromGUID(textComponent.fontGUID, fontPath, textComponent.fontSize);
            //ENGINE_PRINT("[TextSystem] Font reloaded with size: ", textComponent.fontSize, "\n");
        }

        // Sync position and transform from Transform component
        if (ecsManager.HasComponent<Transform>(entity)) {
            Transform& transform = ecsManager.GetComponent<Transform>(entity);

            if (textComponent.is3D) {
                // 3D mode: sync world transform matrix
                textComponent.transform = transform.worldMatrix;
            } else {
                // 2D mode: sync screen space position from Transform
                textComponent.position = Vector3D(transform.worldMatrix.m.m03,
                                                 transform.worldMatrix.m.m13,
                                                 transform.worldMatrix.m.m23);
            }
        }

        // Only submit valid, visible text
        if (textComponent.isVisible && TextUtils::IsValid(textComponent))
        {
            // Create a copy of the text component for submission
            // This ensures the graphics manager has its own copy to work with
            auto textRenderItem = std::make_unique<TextRenderComponent>(textComponent);
            gfxManager.Submit(std::move(textRenderItem));
        }
    }
}

void TextRenderingSystem::Shutdown()
{
    ENGINE_PRINT("[TextSystem] Shutdown\n");
    //std::cout << "[TextSystem] Shutdown" << std::endl;
}
