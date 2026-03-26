#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <memory>
#include <glm/glm.hpp>
#include <Math/Vector3D.hpp>

class Texture;
class Shader;
class VAO;
class EBO;

class SpriteRenderComponent : public IRenderComponent {
public:
    REFL_SERIALIZABLE
    GUID_128 textureGUID{};
    GUID_128 shaderGUID{};

    // Transform properties
    Vector3D position = Vector3D{ 0, 0, 0 };
    Vector3D scale = Vector3D{ 1, 1, 1 };
    float rotation = 0.0f; // In degrees

    // Color tinting
    Vector3D color = Vector3D{ 1.0f, 1.0f, 1.0f };
    float alpha = 1.0f;

    // Rendering properties
    bool is3D = false; // false = screen space, true = world space
    bool enableBillboard = true;
    int sortingLayer = 0; // Sorting layer (higher = drawn on top)
    int sortingOrder = 0; // Order within the sorting layer (higher = drawn on top)
    bool includePostProcess = false; // If true, sprite will be affected by post-processing effects; otherwise, it will be rendered after post-processing

    // Saved 3D position for mode switching
    Vector3D saved3DPosition = Vector3D{ 0.0f, 0.0f, 0.0f };

    // UV coordinates for texture atlasing/sprite sheets
    glm::vec2 uvOffset = glm::vec2(0.0f);
    glm::vec2 uvScale = glm::vec2(1.0f);

    // Fill properties
    int fillMode = 0;          // 0 = Solid (no fill), 1 = Radial, 2 = Horizontal, 3 = Vertical
    int fillDirection = 0;     // 0 = default (left→right / bottom→top), 1 = reverse (right→left / top→bottom)
    float fillMaxValue = 1.0f; // Max value for fill ratio
    float fillValue = 1.0f;    // Current value (fillAmount = fillValue / fillMaxValue)
    float fillGlow = 0.5f;     // Edge glow intensity (0 = off, 1 = max)
    float fillBackground = 0.3f; // Unfilled area brightness (0=hidden, 0.3=dark overlay, 1=full)

    std::shared_ptr<Texture> texture;
    std::shared_ptr<Shader> shader;

    std::string texturePath; // Path to the texture file for display purposes

    // Migration flag to prevent spam
    bool hasMigratedToTransform = false;

    //SpriteRenderComponent(std::shared_ptr<Texture> tex, std::shared_ptr<Shader> s)
    //    : texture(std::move(tex)), shader(std::move(s)) {
    //    renderOrder = 200; // Render after 3D models but before UI
    //}

    SpriteRenderComponent(GUID_128 t_GUID, GUID_128 s_GUID)
        : textureGUID(t_GUID), shaderGUID(s_GUID) {
        renderOrder = 200; // Render after 3D models but before UI
    }

    VAO* spriteVAO = nullptr;
    EBO* spriteEBO = nullptr;

    SpriteRenderComponent() = default;
    ~SpriteRenderComponent() = default;

    // Lua-friendly texture setter (avoids shared_ptr in Lua bindings)
    void SetTextureFromGUID(const std::string& guidString);
};