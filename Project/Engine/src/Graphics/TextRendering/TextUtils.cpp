#include "pch.h"
#include "Graphics/TextRendering/TextUtils.hpp"
#include "Graphics/TextRendering/Font.hpp"

void TextUtils::SetText(TextRenderComponent& comp, const std::string& newText) 
{
    comp.text = newText;
}

void TextUtils::SetColor(TextRenderComponent& comp, const Vector3D& newColor)
{
    comp.color = newColor;
}

void TextUtils::SetColor(TextRenderComponent& comp, float r, float g, float b) 
{
    comp.color = Vector3D(r, g, b);
}

void TextUtils::SetPosition(TextRenderComponent& comp, const Vector3D& newPosition)
{
    comp.position = newPosition;
    comp.is3D = false; // Setting 2D position
}

void TextUtils::SetPosition(TextRenderComponent& comp, float x, float y, float z)
{
    comp.position = Vector3D(x, y, z);
    comp.is3D = false; // Setting 2D position
}

void TextUtils::SetAlignment(TextRenderComponent& comp, TextRenderComponent::Alignment newAlignment)
{
    comp.alignment = newAlignment;
    comp.alignmentInt = static_cast<int>(newAlignment);
}

void TextUtils::SetWorldTransform(TextRenderComponent& comp, const Matrix4x4& newTransform)
{
    comp.transform = newTransform;
    comp.is3D = true; // Automatically set to 3D mode
}

void TextUtils::SetWorldPosition(TextRenderComponent& comp, const Vector3D& worldPos)
{
    comp.transform = Matrix4x4::Identity().Translate(worldPos.x, worldPos.y, worldPos.z);
    comp.is3D = true;
}

void TextUtils::SetWorldPosition(TextRenderComponent& comp, float x, float y, float z)
{
    SetWorldPosition(comp, Vector3D(x, y, z));
}

float TextUtils::GetEstimatedWidth(const TextRenderComponent& comp)
{
    if (comp.font)
    {
        // Use X scale for width (Unity-like behavior)
        return comp.font->GetTextWidth(comp.text, comp.transformScale.x);
    }
    return 0.0f;
}

float TextUtils::GetEstimatedHeight(const TextRenderComponent& comp)
{
    if (comp.font) {
        // Use Y scale for height (Unity-like behavior)
        return comp.font->GetTextHeight(comp.transformScale.y);
    }
    return 0.0f;
}

bool TextUtils::IsValid(const TextRenderComponent& comp) 
{
    return !comp.text.empty() && comp.font && comp.shader;
}

void TextUtils::CenterOnScreen(TextRenderComponent& comp, int screenWidth, int screenHeight)
{
    float centerX = screenWidth / 2.0f;
    float centerY = screenHeight / 2.0f;

    SetPosition(comp, centerX, centerY, 0.0f);
    SetAlignment(comp, TextRenderComponent::Alignment::CENTER);
}

void TextUtils::SetScreenAnchor(TextRenderComponent& comp, int screenWidth, int screenHeight, float anchorX, float anchorY) 
{
    // anchorX and anchorY are 0.0 to 1.0
    // (0,0) = top-left, (1,1) = bottom-right, (0.5,0.5) = center

    float posX = anchorX * screenWidth;
    float posY = anchorY * screenHeight;

    SetPosition(comp, posX, posY, 0.0f);

    // Set alignment based on anchor position
    if (anchorX < 0.33f)
    {
        SetAlignment(comp, TextRenderComponent::Alignment::LEFT);
    }
    else if (anchorX > 0.66f)
    {
        SetAlignment(comp, TextRenderComponent::Alignment::RIGHT);
    }
    else
    {
        SetAlignment(comp, TextRenderComponent::Alignment::CENTER);
    }
}

Vector3D TextUtils::GetTextDimensions(const TextRenderComponent& comp)
{
    return Vector3D(GetEstimatedWidth(comp), GetEstimatedHeight(comp), 0);
}

void TextUtils::SetWordWrap(TextRenderComponent& comp, bool enabled)
{
    if (comp.wordWrap != enabled)
    {
        comp.wordWrap = enabled;
        comp.wrappedLines.clear();
    }
}

bool TextUtils::GetWordWrap(const TextRenderComponent& comp)
{
    return comp.wordWrap;
}

void TextUtils::SetMaxWidth(TextRenderComponent& comp, float maxWidth)
{
    if (comp.maxWidth != maxWidth)
    {
        comp.maxWidth = maxWidth;
        comp.wrappedLines.clear();
    }
}

float TextUtils::GetMaxWidth(const TextRenderComponent& comp)
{
    return comp.maxWidth;
}

void TextUtils::SetLineSpacing(TextRenderComponent& comp, float spacing)
{
    comp.lineSpacing = spacing;
}

float TextUtils::GetLineSpacing(const TextRenderComponent& comp)
{
    return comp.lineSpacing;
}

void TextUtils::EnableWordWrap(TextRenderComponent& comp, float maxWidth, float lineSpacing)
{
    comp.wordWrap = true;
    comp.maxWidth = maxWidth;
    comp.lineSpacing = lineSpacing;
    comp.wrappedLines.clear();
}

void TextUtils::DisableWordWrap(TextRenderComponent& comp)
{
    comp.wordWrap = false;
    comp.wrappedLines.clear();
}