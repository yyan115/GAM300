#include "pch.h"
#include "UI/Anchor/UIAnchorSystem.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "UI/Anchor/UIAnchorComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"

void UIAnchorSystem::Initialise(ECSManager& ecsManager)
{
    m_ecs = &ecsManager;
    ENGINE_PRINT("[UIAnchorSystem] Initialized\n");
}

void UIAnchorSystem::Update()
{
    PROFILE_FUNCTION();
    if (!m_ecs) return;

    // Get current viewport size
    GraphicsManager& gfx = GraphicsManager::GetInstance();
    int viewportWidth, viewportHeight;
    gfx.GetViewportSize(viewportWidth, viewportHeight);

    // Skip if viewport not set yet
    if (viewportWidth <= 0 || viewportHeight <= 0) return;

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities)
    {
        // Skip inactive entities
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        auto& anchor = ecsManager.GetComponent<UIAnchorComponent>(entity);

        // Skip if entity doesn't have a Transform
        if (!ecsManager.HasComponent<Transform>(entity)) {
            continue;
        }

        auto& transform = ecsManager.GetComponent<Transform>(entity);

        // Initialize original scale on first frame
        if (!anchor.hasInitialized)
        {
            anchor.originalScaleX = transform.localScale.x;
            anchor.originalScaleY = transform.localScale.y;
            anchor.hasInitialized = true;
        }

        // Calculate base position from anchor
        float screenX = anchor.anchorX * static_cast<float>(viewportWidth) + anchor.offsetX;
        float screenY = anchor.anchorY * static_cast<float>(viewportHeight) + anchor.offsetY;

        // Handle size modes
        float newScaleX = anchor.originalScaleX;
        float newScaleY = anchor.originalScaleY;

        switch (anchor.sizeMode)
        {
        case UISizeMode::Fixed:
            // Keep original scale, just position
            break;

        case UISizeMode::StretchX:
        {
            // Width stretches to fill between margins
            float availableWidth = static_cast<float>(viewportWidth) - anchor.marginLeft - anchor.marginRight;
            newScaleX = availableWidth;
            // Center horizontally between margins
            screenX = anchor.marginLeft + availableWidth * 0.5f;
            break;
        }

        case UISizeMode::StretchY:
        {
            // Height stretches to fill between margins
            float availableHeight = static_cast<float>(viewportHeight) - anchor.marginBottom - anchor.marginTop;
            newScaleY = availableHeight;
            // Center vertically between margins
            screenY = anchor.marginBottom + availableHeight * 0.5f;
            break;
        }

        case UISizeMode::StretchBoth:
        {
            // Both dimensions stretch to fill between margins
            float availableWidth = static_cast<float>(viewportWidth) - anchor.marginLeft - anchor.marginRight;
            float availableHeight = static_cast<float>(viewportHeight) - anchor.marginBottom - anchor.marginTop;
            newScaleX = availableWidth;
            newScaleY = availableHeight;
            // Position at center of available area
            screenX = anchor.marginLeft + availableWidth * 0.5f;
            screenY = anchor.marginBottom + availableHeight * 0.5f;
            break;
        }

        case UISizeMode::ScaleUniform:
        {
            // Scale uniformly based on screen size vs reference resolution
            float scaleFactorX = static_cast<float>(viewportWidth) / anchor.referenceWidth;
            float scaleFactorY = static_cast<float>(viewportHeight) / anchor.referenceHeight;
            // Use the smaller factor to ensure it fits
            float scaleFactor = std::min(scaleFactorX, scaleFactorY);
            newScaleX = anchor.originalScaleX * scaleFactor;
            newScaleY = anchor.originalScaleY * scaleFactor;
            break;
        }
        }

        // Update Transform position
        // For 2D UI, we use X and Y as screen coordinates, Z stays the same for layering
        transform.localPosition.x = screenX;
        transform.localPosition.y = screenY;
        // Keep Z for sorting/layering

        // Update Transform scale if size mode requires it
        if (anchor.sizeMode != UISizeMode::Fixed)
        {
            transform.localScale.x = newScaleX;
            transform.localScale.y = newScaleY;
        }

        // Mark transform as dirty so it recalculates world matrix
        transform.isDirty = true;
    }

    // Cache viewport size
    m_lastViewportWidth = viewportWidth;
    m_lastViewportHeight = viewportHeight;
}

void UIAnchorSystem::Shutdown()
{
    m_ecs = nullptr;
    ENGINE_PRINT("[UIAnchorSystem] Shutdown\n");
}
