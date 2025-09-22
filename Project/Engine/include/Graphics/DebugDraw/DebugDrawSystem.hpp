#pragma once
#include "ECS/System.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Camera.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/EBO.h"
#include "DebugDrawComponent.hpp"

class Model;

class DebugDrawSystem : public System {
public:
    DebugDrawSystem() = default;
    ~DebugDrawSystem() = default;

    bool Initialise();
    void Update();
    void Shutdown();

    static void DrawCube(const glm::vec3& position, const glm::vec3& scale = glm::vec3(1.0f), const glm::vec3& color = glm::vec3(1.0f), float duration = 0.0f);
    static void DrawSphere(const glm::vec3& position, float radius = 1.0f, const glm::vec3& color = glm::vec3(1.0f), float duration = 0.0f);
    static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color = glm::vec3(1.0f), float duration = 0.0f, float width = 1.0f);
    static void DrawMeshWireframe(std::shared_ptr<Model> model, const glm::vec3& position, const glm::vec3& color = glm::vec3(1.0f), float duration = 0.0f);
    
private:
    static std::vector<DebugDrawData> debugQueue;

    void CreatePrimitiveGeometry();
    void CreateCubeGeometry();
    void CreateSphereGeometry();
    void CreateLineGeometry();

    void SubmitDebugRenderItems();
    void UpdateTimedCommands(float deltaTime);

    std::shared_ptr<Shader> debugShader;

    // Primitive geometry data
    struct GeometryData {
        std::unique_ptr<VAO> vao;
        std::unique_ptr<VBO> vbo;
        std::unique_ptr<EBO> ebo;
        unsigned int indexCount = 0;
    };

    GeometryData cubeGeometry;
    GeometryData sphereGeometry;
    GeometryData lineGeometry;

};