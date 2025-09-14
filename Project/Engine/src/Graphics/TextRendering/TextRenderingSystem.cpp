#include "pch.h"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"

bool TextRenderingSystem::Initialise()
{
	std::cout << "[TextSystem] Initialized" << std::endl;
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
        if (textComponent.isVisible && textComponent.IsValid()) 
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
    std::cout << "[TextSystem] Shutdown" << std::endl;
}
