#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <memory>
#include <glm/glm.hpp>

class Texture;
class Shader;
class VAO;

class SpriteRenderComponent : public IRenderComponent {
public:
	std::shared_ptr<Texture> texture;
	std::shared_ptr<Shader> shader;

    // Transform properties
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    float rotation = 0.0f; // In degrees

    // Color tinting
    glm::vec3 color = glm::vec3(1.0f);
    float alpha = 1.0f;

    // UV coordinates for texture atlasing/sprite sheets
    glm::vec2 uvOffset = glm::vec2(0.0f);
    glm::vec2 uvScale = glm::vec2(1.0f);

    // Rendering properties
    bool is3D = false; // false = screen space, true = world space
    bool enableBillboard = true;
    int layer = 0; // For sorting sprites

    SpriteRenderComponent(std::shared_ptr<Texture> tex, std::shared_ptr<Shader> s)
        : texture(std::move(tex)), shader(std::move(s)) {
        renderOrder = 200; // Render after 3D models but before UI
    }

    VAO* spriteVAO = nullptr;

    SpriteRenderComponent() = default;
    ~SpriteRenderComponent() = default;
};