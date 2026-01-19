#pragma once
#include "Reflection/ReflectionBase.hpp"
#include <Math/Vector3D.hpp>
#include <cmath>

/**
 * @brief Size behavior mode for UI elements when screen aspect ratio changes
 */
enum class UISizeMode
{
    Fixed,          // Keep pixel size constant
    StretchX,       // Stretch width to fill between margins, fixed height
    StretchY,       // Stretch height to fill between margins, fixed width
    StretchBoth,    // Stretch both axes to fill between margins (for backgrounds)
    ScaleUniform    // Scale both axes equally to maintain aspect ratio
};

/**
 * @brief Anchor presets for common UI positions
 */
enum class UIAnchorPreset
{
    Custom,         // Custom anchor values
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

/**
 * @brief Component for anchoring UI elements to screen positions
 *
 * This component works with Transform to position UI elements relative to
 * screen edges/center. The UIAnchorSystem calculates world positions based
 * on anchor settings and viewport size.
 *
 * Coordinate system:
 * - anchor (0,0) = bottom-left of screen
 * - anchor (1,1) = top-right of screen
 * - anchor (0.5, 0.5) = center of screen
 */
struct UIAnchorComponent
{
    REFL_SERIALIZABLE

    // ===== Anchor Position =====
    // Normalized screen position (0-1). The UI element will be positioned
    // relative to this point on the screen.
    float anchorX = 0.5f;   // 0 = left edge, 0.5 = center, 1 = right edge
    float anchorY = 0.5f;   // 0 = bottom edge, 0.5 = center, 1 = top edge

    // Pixel offset from the anchor point
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // ===== Size Mode =====
    UISizeMode sizeMode = UISizeMode::Fixed;

    // ===== Margins (for stretch modes) =====
    // Pixels from screen edge when using stretch modes
    float marginLeft = 0.0f;
    float marginRight = 0.0f;
    float marginTop = 0.0f;
    float marginBottom = 0.0f;

    // ===== Reference Size (for ScaleUniform mode) =====
    // The "design" resolution this UI was created for
    float referenceWidth = 1920.0f;
    float referenceHeight = 1080.0f;

    // ===== Runtime State =====
    // Original scale (captured on first frame, used for ScaleUniform)
    float originalScaleX = 1.0f;
    float originalScaleY = 1.0f;
    bool hasInitialized = false;

    // ===== Helper Methods =====

    // Set anchor from preset
    void SetPreset(UIAnchorPreset preset)
    {
        switch (preset)
        {
        case UIAnchorPreset::TopLeft:       anchorX = 0.0f; anchorY = 1.0f; break;
        case UIAnchorPreset::TopCenter:     anchorX = 0.5f; anchorY = 1.0f; break;
        case UIAnchorPreset::TopRight:      anchorX = 1.0f; anchorY = 1.0f; break;
        case UIAnchorPreset::MiddleLeft:    anchorX = 0.0f; anchorY = 0.5f; break;
        case UIAnchorPreset::Center:        anchorX = 0.5f; anchorY = 0.5f; break;
        case UIAnchorPreset::MiddleRight:   anchorX = 1.0f; anchorY = 0.5f; break;
        case UIAnchorPreset::BottomLeft:    anchorX = 0.0f; anchorY = 0.0f; break;
        case UIAnchorPreset::BottomCenter:  anchorX = 0.5f; anchorY = 0.0f; break;
        case UIAnchorPreset::BottomRight:   anchorX = 1.0f; anchorY = 0.0f; break;
        case UIAnchorPreset::Custom:
        default:
            break; // Keep current values
        }
    }

    // Get current preset (or Custom if doesn't match any preset)
    UIAnchorPreset GetCurrentPreset() const
    {
        constexpr float epsilon = 0.001f;
        auto approxEqual = [epsilon](float a, float b) { return std::abs(a - b) < epsilon; };

        if (approxEqual(anchorX, 0.0f) && approxEqual(anchorY, 1.0f)) return UIAnchorPreset::TopLeft;
        if (approxEqual(anchorX, 0.5f) && approxEqual(anchorY, 1.0f)) return UIAnchorPreset::TopCenter;
        if (approxEqual(anchorX, 1.0f) && approxEqual(anchorY, 1.0f)) return UIAnchorPreset::TopRight;
        if (approxEqual(anchorX, 0.0f) && approxEqual(anchorY, 0.5f)) return UIAnchorPreset::MiddleLeft;
        if (approxEqual(anchorX, 0.5f) && approxEqual(anchorY, 0.5f)) return UIAnchorPreset::Center;
        if (approxEqual(anchorX, 1.0f) && approxEqual(anchorY, 0.5f)) return UIAnchorPreset::MiddleRight;
        if (approxEqual(anchorX, 0.0f) && approxEqual(anchorY, 0.0f)) return UIAnchorPreset::BottomLeft;
        if (approxEqual(anchorX, 0.5f) && approxEqual(anchorY, 0.0f)) return UIAnchorPreset::BottomCenter;
        if (approxEqual(anchorX, 1.0f) && approxEqual(anchorY, 0.0f)) return UIAnchorPreset::BottomRight;

        return UIAnchorPreset::Custom;
    }
};
