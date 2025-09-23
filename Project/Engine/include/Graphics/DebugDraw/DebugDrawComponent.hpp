#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
class VAO;
class Model;

enum class DebugDrawType {
    CUBE,
    SPHERE,
    LINE,
    MESH_WIREFRAME,
    AABB,
    OBB
};

struct DebugDrawData {
    DebugDrawType type;

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);

    float duration = 0.0f;  // 0 = permanent, >0 = timed
    float lineWidth = 1.0f;

    // For line drawing
    glm::vec3 endPosition = glm::vec3(0.0f);

    // For mesh wireframe
    std::shared_ptr<Model> meshModel = nullptr;

    DebugDrawData(DebugDrawType t) : type(t) {}
};

class DebugDrawComponent : public IRenderComponent {
public:
    std::vector<DebugDrawData> drawCommands;
    std::shared_ptr<Shader> shader;  // Debug shader

    // Geometry references (set by system during initialization)
    VAO* cubeVAO = nullptr;
    VAO* sphereVAO = nullptr;
    VAO* lineVAO = nullptr;
    unsigned int cubeIndexCount = 0;
    unsigned int sphereIndexCount = 0;

    DebugDrawComponent() {
        renderOrder = 1000;  // Render after everything else
    }

    // Simple data accessors - no logic
    size_t GetCommandCount() const { return drawCommands.size(); }
    bool HasCommands() const { return !drawCommands.empty(); }
};