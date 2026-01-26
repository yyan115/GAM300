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

    //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Update start, entities=", entities.size(), "\n");

    // Submit all visible text components to the graphics manager
    for (const auto& entity : entities)
    {
        //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Processing entity ", entity, "\n");

        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        auto& textComponent = ecsManager.GetComponent<TextRenderComponent>(entity);
        //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Got TextRenderComponent for entity ", entity, "\n");

        //// Check if font needs to be loaded, or if font/size changed
        //bool fontGUIDChanged = (textComponent.fontGUID != textComponent.lastLoadedFontGUID);
        //bool fontSizeChanged = textComponent.font && (textComponent.font->GetFontSize() != textComponent.fontSize);

        //if (!textComponent.font || fontGUIDChanged) {
        //    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Font load required for entity ", entity, "\n");
        //    try {
        //        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.fontGUID);
        //        ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Font path=", fontPath, "\n");
        //        if (!fontPath.empty()) {
        //            textComponent.font = ResourceManager::GetInstance().GetFontResource(fontPath, textComponent.fontSize);
        //            textComponent.lastLoadedFontGUID = textComponent.fontGUID;
        //            ENGINE_PRINT(EngineLogging::LogLevel::Info, "[TextSystem] Font loaded: ", fontPath, " size=", textComponent.fontSize, "\n");
        //        }
        //    }
        //    catch (const std::exception& e) {
        //        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TextSystem] Failed to load font: ", e.what(), "\n");
        //    }
        //}
        //else if (fontSizeChanged && textComponent.fontSize > 0) {
        //    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Font size changed for entity ", entity, " new size=", textComponent.fontSize, "\n");
        //    try {
        //        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.fontGUID);
        //        ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Reload font path=", fontPath, "\n");
        //        if (!fontPath.empty()) {
        //            std::string resourcePath = MetaFilesManager::GetResourceNameFromAssetFile(fontPath);
        //            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Resource path=", resourcePath, "\n");
        //            if (!resourcePath.empty()) {
        //                auto newFont = std::make_shared<Font>();
        //                if (newFont->LoadResource(resourcePath, fontPath, textComponent.fontSize)) {
        //                    textComponent.font = newFont;
        //                    ENGINE_PRINT(EngineLogging::LogLevel::Info, "[TextSystem] Font reloaded with new size=", textComponent.fontSize, "\n");
        //                }
        //            }
        //        }
        //    }
        //    catch (const std::exception& e) {
        //        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TextSystem] Failed to reload font: ", e.what(), "\n");
        //    }
        //}

        // Sync position, scale and transform from Transform component
        if (ecsManager.HasComponent<Transform>(entity)) {
            Transform& transform = ecsManager.GetComponent<Transform>(entity);
            //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Syncing transform for entity ", entity, "\n");

            textComponent.transformScale = transform.localScale;

            if (textComponent.is3D) {
                textComponent.transform = transform.worldMatrix;
                //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Entity ", entity, " in 3D mode\n");
            }
            else {
                textComponent.position = Vector3D(transform.worldMatrix.m.m03,
                    transform.worldMatrix.m.m13,
                    transform.worldMatrix.m.m23);
                //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Entity ", entity, " in 2D mode, pos=", textComponent.position.x, ",", textComponent.position.y, ",", textComponent.position.z, "\n");
            }
        }

        // Only submit valid, visible text
        if (textComponent.isVisible && TextUtils::IsValid(textComponent))
        {
            //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Submitting text for entity ", entity, "\n");
            auto textRenderItem = std::make_unique<TextRenderComponent>(textComponent);
            gfxManager.Submit(std::move(textRenderItem));
        }
        else {
            //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Entity ", entity, " not visible or invalid, skipping\n");
        }
    }

    //ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[TextSystem] Update end\n");
}


void TextRenderingSystem::Shutdown()
{
    ENGINE_PRINT("[TextSystem] Shutdown\n");
    //std::cout << "[TextSystem] Shutdown" << std::endl;
}
