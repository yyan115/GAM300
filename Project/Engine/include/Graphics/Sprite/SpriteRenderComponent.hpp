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
    int layer = 0; // For sorting sprites

    // Saved 3D position for mode switching
    Vector3D saved3DPosition = Vector3D{ 0.0f, 0.0f, 0.0f };

    // UV coordinates for texture atlasing/sprite sheets
    glm::vec2 uvOffset = glm::vec2(0.0f);
    glm::vec2 uvScale = glm::vec2(1.0f);

    std::shared_ptr<Texture> texture;
    std::shared_ptr<Shader> shader;

    std::string texturePath; // Path to the texture file for display purposes

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
};