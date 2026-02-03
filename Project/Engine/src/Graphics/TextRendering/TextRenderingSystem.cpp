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
        textComp.lastLoadedFontGUID = textComp.fontGUID;
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

    for (const auto& entity : entities)
    {
        // Skip entities that are inactive in hierarchy
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) 
        {
            continue;
        }

        auto& textComponent = ecsManager.GetComponent<TextRenderComponent>(entity);

        // Sync alignment enum from alignmentInt (alignmentInt is serialized, alignment enum is used for rendering)
        textComponent.alignment = static_cast<TextRenderComponent::Alignment>(textComponent.alignmentInt);

        // Load or reload font if needed (handles newly added components, fontGUID changes, or fontSize changes)
        bool needsFontReload = !textComponent.font ||
                               textComponent.fontGUID != textComponent.lastLoadedFontGUID ||
                               (textComponent.font && textComponent.font->GetFontSize() != textComponent.fontSize);
        bool hasValidFontGUID = textComponent.fontGUID.high != 0 || textComponent.fontGUID.low != 0;
        if (needsFontReload && hasValidFontGUID)
        {
            std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.fontGUID);
            textComponent.font = ResourceManager::GetInstance().GetFontResourceFromGUID(textComponent.fontGUID, fontPath, textComponent.fontSize);
            textComponent.lastLoadedFontGUID = textComponent.fontGUID;
        }

        // Ensure shader is loaded (handles newly added components or missing shader)
        if (!textComponent.shader)
        {
#ifndef ANDROID
            std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(textComponent.shaderGUID);
            textComponent.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(textComponent.shaderGUID, shaderPath);
#else
            std::string shaderPath = ResourceManager::GetPlatformShaderPath("text");
            textComponent.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
#endif
        }

        // Sync position, scale and transform from Transform component
        if (ecsManager.HasComponent<Transform>(entity)) 
        {
            Transform& transform = ecsManager.GetComponent<Transform>(entity);

            textComponent.transformScale = transform.localScale;

            if (textComponent.is3D) 
            {
                textComponent.transform = transform.worldMatrix;
            }
            else 
            {
                textComponent.position = Vector3D(transform.worldMatrix.m.m03,
                    transform.worldMatrix.m.m13,
                    transform.worldMatrix.m.m23);
            }
        }

        // Compute wrapped lines before submission
        if (textComponent.font) 
        {
            float worldScaleFactor = textComponent.is3D ? 0.01f : 1.0f;
            float scaleX = textComponent.is3D ? worldScaleFactor : (textComponent.transformScale.x * worldScaleFactor);
            ComputeWrappedLines(textComponent, scaleX);
        }

        // Sync sorting values to renderOrder
        textComponent.renderOrder = textComponent.sortingLayer * 100 + textComponent.sortingOrder;

        if (textComponent.isVisible && TextUtils::IsValid(textComponent))
        {
            auto textRenderItem = std::make_unique<TextRenderComponent>(textComponent);
            gfxManager.Submit(std::move(textRenderItem));
        }
    }
}

void TextRenderingSystem::Shutdown()
{
    ENGINE_PRINT("[TextSystem] Shutdown\n");
}

void TextRenderingSystem::ComputeWrappedLines(TextRenderComponent& comp, float scaleX)
{
    comp.wrappedLines.clear();

    // If word wrap is disabled or no max width, return single line
    if (!comp.wordWrap || comp.maxWidth <= 0.0f || !comp.font) {
        comp.wrappedLines.push_back(comp.text);
        return;
    }

    comp.wrappedLines = WrapText(comp.text, comp.font.get(), comp.maxWidth, scaleX);
}

std::vector<std::string> TextRenderingSystem::WrapText(const std::string& text, Font* font, float maxWidth, float scaleX)
{
    std::vector<std::string> result;

    if (!font || text.empty()) {
        result.push_back(text);
        return result;
    }

    // Split text into words (preserving spaces for proper spacing)
    std::vector<std::string> words;
    std::string currentWord;

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];

        // Handle explicit newlines
        if (c == '\n') {
            if (!currentWord.empty()) {
                words.push_back(currentWord);
                currentWord.clear();
            }
            words.push_back("\n");  // Special marker for newline
        }
        // Space marks end of word
        else if (c == ' ') {
            if (!currentWord.empty()) {
                words.push_back(currentWord);
                currentWord.clear();
            }
            words.push_back(" ");
        }
        else {
            currentWord += c;
        }
    }

    // Don't forget the last word
    if (!currentWord.empty()) {
        words.push_back(currentWord);
    }

    // Build lines respecting maxWidth
    std::string currentLine;
    float currentLineWidth = 0.0f;
    float spaceWidth = font->GetTextWidth(" ", scaleX);

    for (const std::string& word : words) {
        // Handle explicit newlines
        if (word == "\n") {
            result.push_back(currentLine);
            currentLine.clear();
            currentLineWidth = 0.0f;
            continue;
        }

        // Handle spaces
        if (word == " ") {
            if (!currentLine.empty()) {
                float newWidth = currentLineWidth + spaceWidth;
                if (newWidth <= maxWidth) {
                    currentLine += " ";
                    currentLineWidth = newWidth;
                }
            }
            continue;
        }

        // Calculate word width
        float wordWidth = font->GetTextWidth(word, scaleX);

        // Check if word fits on current line
        float widthNeeded = currentLine.empty() ? wordWidth : currentLineWidth + spaceWidth + wordWidth;

        if (widthNeeded <= maxWidth) {
            // Word fits
            if (!currentLine.empty()) {
                currentLine += " ";
                currentLineWidth += spaceWidth;
            }
            currentLine += word;
            currentLineWidth += wordWidth;
        }
        else {
            // Word doesn't fit
            if (currentLine.empty()) {
                // Word is too long - break character by character
                std::string brokenWord;
                float brokenWidth = 0.0f;

                for (char c : word) {
                    float charWidth = font->GetTextWidth(std::string(1, c), scaleX);

                    if (brokenWidth + charWidth > maxWidth && !brokenWord.empty()) {
                        result.push_back(brokenWord);
                        brokenWord.clear();
                        brokenWidth = 0.0f;
                    }

                    brokenWord += c;
                    brokenWidth += charWidth;
                }

                currentLine = brokenWord;
                currentLineWidth = brokenWidth;
            }
            else {
                // Push current line and start new one
                result.push_back(currentLine);
                currentLine = word;
                currentLineWidth = wordWidth;
            }
        }
    }

    // Don't forget the last line
    if (!currentLine.empty()) {
        result.push_back(currentLine);
    }

    if (result.empty()) {
        result.push_back("");
    }

    return result;
}

int TextRenderingSystem::GetLineCount(const TextRenderComponent& comp, float scaleX)
{
    if (!comp.font || comp.text.empty())
        return 0;

    // Use cached lines if available, otherwise compute temporarily
    if (!comp.wrappedLines.empty()) {
        return static_cast<int>(comp.wrappedLines.size());
    }

    auto lines = WrapText(comp.text, comp.font.get(), comp.maxWidth, scaleX);
    return static_cast<int>(lines.size());
}

float TextRenderingSystem::GetTotalHeight(const TextRenderComponent& comp, float scaleX)
{
    if (!comp.font)
        return 0.0f;

    int lineCount = GetLineCount(comp, scaleX);
    if (lineCount == 0)
        return 0.0f;

    float scaleY = comp.is3D ? 0.01f : (comp.transformScale.y);
    float singleLineHeight = comp.font->GetTextHeight(scaleY);
    float lineHeight = singleLineHeight * comp.lineSpacing;

    return (lineCount - 1) * lineHeight + singleLineHeight;
}