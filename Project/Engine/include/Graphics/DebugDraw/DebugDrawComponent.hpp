#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <memory>
#include <vector>
#include "Math/Vector3D.hpp"
#include "Utilities/GUID.hpp"

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
    REFL_SERIALIZABLE //Change glm to Vector3
    DebugDrawType type;

    Vector3D position{};
    Vector3D scale{};
    Vector3D rotation{};
    Vector3D color{};

    float duration = 0.0f;  // 0 = permanent, >0 = timed
    float lineWidth = 1.0f;

    // For line drawing
    Vector3D endPosition{};

    // For mesh wireframe
    GUID_128 modelGUID;
    std::shared_ptr<Model> meshModel = nullptr; // don't have to serialize for now

    DebugDrawData(DebugDrawType t) : type(t) {}
};

class DebugDrawComponent : public IRenderComponent {
public:
    REFL_SERIALIZABLE
    std::vector<DebugDrawData> drawCommands;
    std::shared_ptr<Shader> shader;  // Debug shader - Should be changed to ID for reflection

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