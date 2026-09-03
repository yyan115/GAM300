#pragma once

#include "Graphics/Sprite/SpriteRenderComponent.hpp"

// Frame-local render state. The ECS component owns the referenced resources
// until rendering finishes, so copying GUIDs and editor-only strings is wasteful.
class SpriteRenderItem final : public IRenderComponent {
public:
    Vector3D position{};
    Vector3D scale{1.0f, 1.0f, 1.0f};
    Vector3D color{1.0f, 1.0f, 1.0f};
    float rotation = 0.0f;
    float alpha = 1.0f;
    bool is3D = false;
    bool enableBillboard = true;
    glm::vec2 uvOffset{0.0f};
    glm::vec2 uvScale{1.0f};
    int fillMode = 0;
    int fillDirection = 0;
    float fillMaxValue = 1.0f;
    float fillValue = 1.0f;
    float fillGlow = 0.5f;
    float fillBackground = 0.3f;
    Texture* texture = nullptr;
    Shader* shader = nullptr;
    VAO* spriteVAO = nullptr;

    SpriteRenderItem() = default;
    explicit SpriteRenderItem(const SpriteRenderComponent& source) { Capture(source); }

    void Capture(const SpriteRenderComponent& source)
    {
        isVisible = source.isVisible;
        renderOrder = source.renderOrder;
        excludeFromPostProcess = source.excludeFromPostProcess;
        bloomColor = source.bloomColor;
        bloomIntensity = source.bloomIntensity;
        brightnessBoost = source.brightnessBoost;
        position = source.position;
        scale = source.scale;
        color = source.color;
        rotation = source.rotation;
        alpha = source.alpha;
        is3D = source.is3D;
        enableBillboard = source.enableBillboard;
        uvOffset = source.uvOffset;
        uvScale = source.uvScale;
        fillMode = source.fillMode;
        fillDirection = source.fillDirection;
        fillMaxValue = source.fillMaxValue;
        fillValue = source.fillValue;
        fillGlow = source.fillGlow;
        fillBackground = source.fillBackground;
        texture = source.texture.get();
        shader = source.shader.get();
        spriteVAO = source.spriteVAO;
    }

    RenderComponentKind GetRenderKind() const override
    {
        return RenderComponentKind::SpriteRenderItem;
    }
};
