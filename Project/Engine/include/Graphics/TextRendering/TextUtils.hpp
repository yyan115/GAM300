#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "TextRenderComponent.hpp"
#include "Math/Matrix4x4.hpp"

class Font;

class TextUtils {
public:
    // Text content manipulation
    static void SetText(TextRenderComponent& comp, const std::string& newText);

    // Color utilities
    static void SetColor(TextRenderComponent& comp, const Vector3D& newColor);
    static void SetColor(TextRenderComponent& comp, float r, float g, float b);

    // Position utilities (2D screen space)
    static void SetPosition(TextRenderComponent& comp, const Vector3D& newPosition);
    static void SetPosition(TextRenderComponent& comp, float x, float y, float z = 0.0f);

    // Alignment (Note: Scale is now controlled via Transform component)
    static void SetAlignment(TextRenderComponent& comp, TextRenderComponent::Alignment newAlignment);

    // 3D world space positioning
    static void SetWorldTransform(TextRenderComponent& comp, const Matrix4x4& newTransform);
    static void SetWorldPosition(TextRenderComponent& comp, const Vector3D& worldPos);
    static void SetWorldPosition(TextRenderComponent& comp, float x, float y, float z);

    // Dimension calculations
    static float GetEstimatedWidth(const TextRenderComponent& comp);
    static float GetEstimatedHeight(const TextRenderComponent& comp);

    // Validation
    static bool IsValid(const TextRenderComponent& comp);

    // Advanced utilities
    static void CenterOnScreen(TextRenderComponent& comp, int screenWidth, int screenHeight);
    static void SetScreenAnchor(TextRenderComponent& comp, int screenWidth, int screenHeight,
        float anchorX, float anchorY); // 0.0-1.0 values
    static Vector3D GetTextDimensions(const TextRenderComponent& comp);

    // =========================================================================
    // LINE WRAPPING UTILITIES
    // =========================================================================

    // Enable/disable word wrapping
    static void SetWordWrap(TextRenderComponent& comp, bool enabled);
    static bool GetWordWrap(const TextRenderComponent& comp);

    // Set maximum width for line wrapping (in pixels for 2D, world units for 3D)
    static void SetMaxWidth(TextRenderComponent& comp, float maxWidth);
    static float GetMaxWidth(const TextRenderComponent& comp);

    // Set line spacing multiplier (1.0 = single spacing, 1.5 = 1.5x, 2.0 = double)
    static void SetLineSpacing(TextRenderComponent& comp, float spacing);
    static float GetLineSpacing(const TextRenderComponent& comp);

    // Convenience: Enable word wrap with a specific max width
    static void EnableWordWrap(TextRenderComponent& comp, float maxWidth, float lineSpacing = 1.2f);

    // Convenience: Disable word wrap
    static void DisableWordWrap(TextRenderComponent& comp);
};