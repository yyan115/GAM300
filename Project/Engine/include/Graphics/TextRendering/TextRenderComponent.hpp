#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "Graphics/TextRendering/Font.hpp"

class Shader;

class TextRenderComponent : public IRenderComponent {
public:
    std::string text;
    std::shared_ptr<Font> font;
    std::shared_ptr<Shader> shader;
    glm::vec3 position{ 0.0f };
    glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    float scale = 1.0f;
    bool is3D = false; // false for screen space, true for world space
    glm::mat4 transform{ 1.0f }; // Used for 3D text positioning

    // Text alignment options
    enum class Alignment {
        LEFT,
        CENTER,
        RIGHT
    };
    Alignment alignment = Alignment::LEFT;

    // Constructor with required parameters
    TextRenderComponent(const std::string& t, std::shared_ptr<Font> f, std::shared_ptr<Shader> s)
        : text(t), font(std::move(f)), shader(std::move(s)) {
        renderOrder = 1000; // Render text after most 3D objects by default
    }
    
    // Copy constructor for when we need to create copies for submission
    TextRenderComponent(const TextRenderComponent& other)
        : IRenderComponent(other), // Copy base class members (isVisible, renderOrder)
        text(other.text),
        font(other.font),
        shader(other.shader),
        position(other.position),
        color(other.color),
        scale(other.scale),
        is3D(other.is3D),
        transform(other.transform),
        alignment(other.alignment) {
    }

    // Assignment operator
    TextRenderComponent& operator=(const TextRenderComponent& other) {
        if (this != &other) {
            IRenderComponent::operator=(other);
            text = other.text;
            font = other.font;
            shader = other.shader;
            position = other.position;
            color = other.color;
            scale = other.scale;
            is3D = other.is3D;
            transform = other.transform;
            alignment = other.alignment;
        }
        return *this;
    }

    // Default constructor for ECS requirements
    TextRenderComponent() = default;
    ~TextRenderComponent() = default;

    // Utility methods
    bool IsValid() const 
    {
        return !text.empty() && font && shader;
    }

    void SetText(const std::string& newText) 
    {
        text = newText;
    }

    void SetColor(const glm::vec3& newColor) 
    {
        color = newColor;
    }

    void SetColor(float r, float g, float b) 
    {
        color = glm::vec3(r, g, b);
    }

    void SetPosition(const glm::vec3& newPosition) 
    {
        position = newPosition;
    }

    void SetPosition(float x, float y, float z = 0.0f) 
    {
        position = glm::vec3(x, y, z);
    }

    void SetScale(float newScale) 
    {
        scale = newScale;
    }

    void SetAlignment(Alignment newAlignment) 
    {
        alignment = newAlignment;
    }

    // For 3D text positioning
    void SetWorldTransform(const glm::mat4& newTransform) 
    {
        transform = newTransform;
        is3D = true; // Automatically set to 3D mode
    }

    void SetWorldPosition(const glm::vec3& worldPos) 
    {
        transform = glm::translate(glm::mat4(1.0f), worldPos);
        is3D = true;
    }

    void SetWorldPosition(float x, float y, float z) 
    {
        SetWorldPosition(glm::vec3(x, y, z));
    }

    // Get estimated text dimensions (useful for UI layout)
    float GetEstimatedWidth() const 
    {
        if (font) 
        {
            return font->GetTextWidth(text, scale);
        }
        return 0.0f;
    }

    float GetEstimatedHeight() const 
    {
        if (font) 
        {
            return font->GetTextHeight(scale);
        }
        return 0.0f;
    }
};