#pragma once
#include "ECS/System.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Camera.h"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/EBO.h"
#include "DebugDrawComponent.hpp"
#include "Engine.h"  // For ENGINE_API macro

class Model;

class ENGINE_API DebugDrawSystem : public System {
public:
    DebugDrawSystem() = default;
    ~DebugDrawSystem() = default;

    bool Initialise();
    void Update();
    void Shutdown();

    static void DrawCube(const Vector3D& position, const Vector3D& scale = Vector3D(1.0f, 1.0f, 1.0f), const Vector3D& color = Vector3D(1.0f, 1.0f, 1.0f), float duration = 0.0f);
    static void DrawSphere(const Vector3D& position, float radius = 1.0f, const Vector3D& color = Vector3D(1.0f, 1.0f, 1.0f), float duration = 0.0f);
    static void DrawLine(const Vector3D& start, const Vector3D& end, const Vector3D& color = Vector3D(1.0f, 1.0f, 1.0f), float duration = 0.0f, float width = 1.0f);
    static void DrawMeshWireframe(std::shared_ptr<Model> model, const Vector3D& position, const Vector3D& color = Vector3D(1.0f, 1.0f, 1.0f), float duration = 0.0f);
    
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