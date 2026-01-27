#pragma once
#include "ECS/System.hpp"

class TextRenderComponent;
class Font;

class TextRenderingSystem : public System {
public:
    TextRenderingSystem() = default;
    ~TextRenderingSystem() = default;

    bool Initialise();
    void Update();
    void Shutdown();

    // Word wrapping utilities - call these to compute wrapped lines for a component
    static void ComputeWrappedLines(TextRenderComponent& comp, float scaleX);

    // Get line count for a text component (computes wrapping if needed)
    static int GetLineCount(const TextRenderComponent& comp, float scaleX);

    // Get total height including all wrapped lines
    static float GetTotalHeight(const TextRenderComponent& comp, float scaleX);

private:
    // Internal helper for word wrapping algorithm
    static std::vector<std::string> WrapText(const std::string& text, Font* font, float maxWidth, float scaleX);
};