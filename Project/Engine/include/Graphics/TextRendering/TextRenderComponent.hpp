#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "Graphics/TextRendering/Font.hpp"
#include "Math/Matrix4x4.hpp"
#include "Utilities/GUID.hpp"

class Shader;

class TextRenderComponent : public IRenderComponent {
public:
    REFL_SERIALIZABLE
    std::string text;
    unsigned int fontSize;
    GUID_128 fontGUID;
    GUID_128 shaderGUID;
    Vector3D position{};
    Vector3D color{ 1.0f, 1.0f, 1.0f };
    float scale = 1.0f;
    bool is3D = false; // false for screen space, true for world space
    Matrix4x4 transform; // Used for 3D text positioning

    std::shared_ptr<Font> font;
    std::shared_ptr<Shader> shader;

    // Text alignment options
    enum class Alignment : int {
        LEFT,
        CENTER,
        RIGHT
    };
    Alignment alignment = Alignment::LEFT;
    int alignmentInt = 0;

    // Constructor with required parameters
    TextRenderComponent(const std::string& t, unsigned int _fontSize, GUID_128 f_GUID, GUID_128 s_GUID)
        : text(t), fontSize(_fontSize), fontGUID(f_GUID), shaderGUID(s_GUID) {
        renderOrder = 1000; // Render text after most 3D objects by default
    }
    
    // Copy constructor for when we need to create copies for submission
    TextRenderComponent(const TextRenderComponent& other)
        : IRenderComponent(other), // Copy base class members (isVisible, renderOrder)
        text(other.text),
        fontSize(other.fontSize),
        fontGUID(other.fontGUID),
        shaderGUID(other.shaderGUID),
        position(other.position),
        color(other.color),
        scale(other.scale),
        is3D(other.is3D),
        transform(other.transform),
        alignment(other.alignment),
        alignmentInt(other.alignmentInt),
        font(other.font),
        shader(other.shader) {
    }

    // Assignment operator
    TextRenderComponent& operator=(const TextRenderComponent& other) {
        if (this != &other) {
            IRenderComponent::operator=(other);
            text = other.text;
            fontSize = other.fontSize;
            fontGUID = other.fontGUID;
            shaderGUID = other.shaderGUID;
            font = other.font;
            shader = other.shader;
            position = other.position;
            color = other.color;
            scale = other.scale;
            is3D = other.is3D;
            transform = other.transform;
            alignment = other.alignment;
            alignmentInt = other.alignmentInt;
        }
        return *this;
    }

    // Default constructor for ECS requirements
    TextRenderComponent() = default;
    ~TextRenderComponent() = default;
};