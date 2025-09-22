#include "pch.h"
#include "Graphics/TextRendering/TextRenderingSystem.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/TextRendering/TextUtils.hpp"
#ifdef ANDROID
#include <android/log.h>
#endif

bool TextRenderingSystem::Initialise()
{
	std::cout << "[TextSystem] Initialized" << std::endl;
	return true;
}

void TextRenderingSystem::Update()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "TextSystem: Processing %zu text entities", entities.size());
#endif

    // Submit all visible text components to the graphics manager
    for (const auto& entity : entities)
    {
        auto& textComponent = ecsManager.GetComponent<TextRenderComponent>(entity);

#ifdef ANDROID
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "TextSystem: Entity %u - isVisible=%d, isValid=%d",
            entity, textComponent.isVisible, TextUtils::IsValid(textComponent));
#endif

        // Only submit valid, visible text
        if (textComponent.isVisible && TextUtils::IsValid(textComponent))
        {
#ifdef ANDROID
            __android_log_print(ANDROID_LOG_INFO, "GAM300", "TextSystem: Submitting text '%s' to graphics manager", textComponent.text.c_str());
#endif
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
