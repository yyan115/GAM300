#include "pch.h"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/TextRendering/TextUtils.hpp"
#include <Asset Manager/AssetManager.hpp>

bool TextRenderingSystem::Initialise()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    for (const auto& entity : entities) {
        auto& textComp = ecsManager.GetComponent<TextRenderComponent>(entity);
        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComp.fontGUID);
        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComp.shaderGUID);
        textComp.font = ResourceManager::GetInstance().GetFontResourceFromGUID(textComp.fontGUID, fontPath, textComp.fontSize);
        textComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(textComp.shaderGUID, shaderPath);
    }

    ENGINE_PRINT("[TextSystem] Initialized\n");
	return true;
}

void TextRenderingSystem::Update()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Submit all visible text components to the graphics manager
    for (const auto& entity : entities) 
    {
        auto& textComponent = ecsManager.GetComponent<TextRenderComponent>(entity);

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
