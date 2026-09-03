#pragma once

#include "Graphics/TextRendering/TextRenderComponent.hpp"

// Frame-local view of text render state. Text and wrapping storage remains in
// the ECS component and is stable from Draw() through deferred rendering.
class TextRenderItem final : public IRenderComponent {
public:
    const std::string* text = nullptr;
    const std::vector<std::string>* wrappedLines = nullptr;
    Vector3D position{};
    Vector3D color{1.0f, 1.0f, 1.0f};
    float alpha = 1.0f;
    bool is3D = false;
    Matrix4x4 transform;
    Vector3D transformScale{1.0f, 1.0f, 1.0f};
    float lineSpacing = 1.2f;
    Font* font = nullptr;
    Shader* shader = nullptr;
    TextRenderComponent::Alignment alignment = TextRenderComponent::Alignment::LEFT;

    TextRenderItem() = default;
    explicit TextRenderItem(const TextRenderComponent& source) { Capture(source); }

    void Capture(const TextRenderComponent& source)
    {
        isVisible = source.isVisible;
        renderOrder = source.renderOrder;
        excludeFromPostProcess = source.excludeFromPostProcess;
        bloomColor = source.bloomColor;
        bloomIntensity = source.bloomIntensity;
        brightnessBoost = source.brightnessBoost;
        text = &source.text;
        wrappedLines = &source.wrappedLines;
        position = source.position;
        color = source.color;
        alpha = source.alpha;
        is3D = source.is3D;
        transform = source.transform;
        transformScale = source.transformScale;
        lineSpacing = source.lineSpacing;
        font = source.font.get();
        shader = source.shader.get();
        alignment = source.alignment;
    }

    RenderComponentKind GetRenderKind() const override
    {
        return RenderComponentKind::TextRenderItem;
    }
};
