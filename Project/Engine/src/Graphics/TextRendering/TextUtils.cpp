#include "pch.h"
#include "Graphics/TextRendering/TextUtils.hpp"
#include "Graphics/TextRendering/Font.hpp"
#ifdef ANDROID
#include <android/log.h>
#endif

void TextUtils::SetText(TextRenderComponent& comp, const std::string& newText) 
{
    comp.text = newText;
}

void TextUtils::SetColor(TextRenderComponent& comp, const glm::vec3& newColor) 
{
    comp.color = newColor;
}

void TextUtils::SetColor(TextRenderComponent& comp, float r, float g, float b) 
{
    comp.color = glm::vec3(r, g, b);
}

void TextUtils::SetPosition(TextRenderComponent& comp, const glm::vec3& newPosition) 
{
    comp.position = newPosition;
    comp.is3D = false; // Setting 2D position
}

void TextUtils::SetPosition(TextRenderComponent& comp, float x, float y, float z)
{
    comp.position = glm::vec3(x, y, z);
    comp.is3D = false; // Setting 2D position
}

void TextUtils::SetScale(TextRenderComponent& comp, float newScale) 
{
    comp.scale = newScale;
}

void TextUtils::SetAlignment(TextRenderComponent& comp, TextRenderComponent::Alignment newAlignment)
{
    comp.alignment = newAlignment;
}

void TextUtils::SetWorldTransform(TextRenderComponent& comp, const glm::mat4& newTransform)
{
    comp.transform = newTransform;
    comp.is3D = true; // Automatically set to 3D mode
}

void TextUtils::SetWorldPosition(TextRenderComponent& comp, const glm::vec3& worldPos) 
{
    comp.transform = glm::translate(glm::mat4(1.0f), worldPos);
    comp.is3D = true;
}

void TextUtils::SetWorldPosition(TextRenderComponent& comp, float x, float y, float z)
{
    SetWorldPosition(comp, glm::vec3(x, y, z));
}

float TextUtils::GetEstimatedWidth(const TextRenderComponent& comp) 
{
    if (comp.font) 
    {
        return comp.font->GetTextWidth(comp.text, comp.scale);
    }
    return 0.0f;
}

float TextUtils::GetEstimatedHeight(const TextRenderComponent& comp)
{
    if (comp.font) {
        return comp.font->GetTextHeight(comp.scale);
    }
    return 0.0f;
}

bool TextUtils::IsValid(const TextRenderComponent& comp)
{
#ifdef ANDROID
    bool textValid = !comp.text.empty();
    bool fontValid = comp.font != nullptr;
    bool shaderValid = comp.shader != nullptr;

    __android_log_print(ANDROID_LOG_INFO, "GAM300", "TextUtils::IsValid - text='%s' empty=%d, font=%p, shader=%p",
        comp.text.c_str(), comp.text.empty(), comp.font.get(), comp.shader.get());
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "TextUtils::IsValid - textValid=%d, fontValid=%d, shaderValid=%d, overall=%d",
        textValid, fontValid, shaderValid, (textValid && fontValid && shaderValid));

    return textValid && fontValid && shaderValid;
#else
    return !comp.text.empty() && comp.font && comp.shader;
#endif
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

glm::vec2 TextUtils::GetTextDimensions(const TextRenderComponent& comp) 
{
    return glm::vec2(GetEstimatedWidth(comp), GetEstimatedHeight(comp));
}