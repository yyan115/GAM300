#include "pch.h"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/TextRendering/TextUtils.hpp"
#include "Transform/TransformComponent.hpp"
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ActiveComponent.hpp"

bool TextRenderingSystem::Initialise()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    for (const auto& entity : entities) {
        auto& textComp = ecsManager.GetComponent<TextRenderComponent>(entity);
        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComp.fontGUID);
        textComp.font = ResourceManager::GetInstance().GetFontResourceFromGUID(textComp.fontGUID, fontPath, textComp.fontSize);
        textComp.lastLoadedFontGUID = textComp.fontGUID; // Remember which font we loaded
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
        // Skip inactive entities
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue; // Don't render inactive entities
            }
        }

        auto& textComponent = ecsManager.GetComponent<TextRenderComponent>(entity);

        // Check if font needs to be loaded, or if font/size changed
        bool fontGUIDChanged = (textComponent.fontGUID != textComponent.lastLoadedFontGUID);
        bool fontSizeChanged = textComponent.font && (textComponent.font->GetFontSize() != textComponent.fontSize);

        if (!textComponent.font || fontGUIDChanged) {
            // Font needs initial load or font was changed in inspector
            try {
                std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.fontGUID);
                if (!fontPath.empty()) {
                    textComponent.font = ResourceManager::GetInstance().GetFontResource(fontPath, textComponent.fontSize);
                    textComponent.lastLoadedFontGUID = textComponent.fontGUID; // Remember which font we loaded
                    //ENGINE_PRINT("[TextSystem] Font loaded: ", fontPath, " with size: ", textComponent.fontSize, "\n");
                }
            }
            catch (const std::exception& e) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TextSystem] Failed to load font: ", e.what(), "\n");
            }
        }
        else if (fontSizeChanged && textComponent.fontSize > 0) {
            // Only font size changed - create a new Font instance directly (not cached)
            // This avoids cache corruption and race conditions
            try {
                std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.fontGUID);
                if (!fontPath.empty()) {
                    std::string resourcePath = MetaFilesManager::GetResourceNameFromAssetFile(fontPath);
                    if (!resourcePath.empty()) {
                        auto newFont = std::make_shared<Font>();
                        if (newFont->LoadResource(resourcePath, fontPath, textComponent.fontSize)) {
                            textComponent.font = newFont;
                            //ENGINE_PRINT("[TextSystem] Font size changed to: ", textComponent.fontSize, "\n");
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TextSystem] Failed to reload font: ", e.what(), "\n");
            }
        }

        // Sync position, scale and transform from Transform component
        if (ecsManager.HasComponent<Transform>(entity)) {
            Transform& transform = ecsManager.GetComponent<Transform>(entity);

            // Always sync the scale from Transform (used for both 2D and 3D)
            textComponent.transformScale = transform.localScale;

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
