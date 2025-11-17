#include "pch.h"
#include "Graphics/DebugDraw/DebugDrawSystem.hpp"
#include "Graphics/DebugDraw/DebugDrawComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Transform/TransformComponent.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "Asset Manager/ResourceManager.hpp"
#include "Logging.hpp"
#include "Performance/PerformanceProfiler.hpp"

std::vector<DebugDrawData> DebugDrawSystem::debugQueue; 

bool DebugDrawSystem::Initialise() 
{
    // Create debug shader (you'll need to create this shader file)
    debugShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("debug"));

    CreatePrimitiveGeometry();
    ENGINE_PRINT("[DebugDrawSystem] Initialized", "\n");
    return true;
}

void DebugDrawSystem::Update()
{
    PROFILE_FUNCTION();
    if (debugQueue.empty()) return;

    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Create component and check all pointers are valid
    auto debugComponent = std::make_unique<DebugDrawComponent>();
    auto shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("debug"));

    if (!shader || !cubeGeometry.vao || !sphereGeometry.vao || !lineGeometry.vao) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Error: Required debug resources are null!\n");
        debugQueue.clear();
        return;
    }

    debugComponent->shader = shader;
    debugComponent->cubeVAO = cubeGeometry.vao.get();
    debugComponent->sphereVAO = sphereGeometry.vao.get();
    debugComponent->lineVAO = lineGeometry.vao.get();
    debugComponent->cubeIndexCount = cubeGeometry.indexCount;
    debugComponent->sphereIndexCount = sphereGeometry.indexCount;

    // Copy instead of move to avoid potential issues
    debugComponent->drawCommands = debugQueue;

    gfxManager.Submit(std::move(debugComponent));

    debugQueue.clear();
}

void DebugDrawSystem::Shutdown()
{
    ENGINE_PRINT("[DebugDrawSystem] Shutdown\n");
}

void DebugDrawSystem::CreatePrimitiveGeometry()
{
    CreateCubeGeometry();
    CreateSphereGeometry();
    CreateLineGeometry();
}

void DebugDrawSystem::CreateCubeGeometry()
{
    std::vector<glm::vec3> vertices = {
        // Bottom face
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f},
        // Top face  
        {-0.5f,  0.5f, -0.5f}, {0.5f,  0.5f, -0.5f},
        {0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}
    };

    std::vector<GLuint> indices = {
        // Bottom face
        0, 1, 1, 2, 2, 3, 3, 0,
        // Top face
        4, 5, 5, 6, 6, 7, 7, 4,
        // Vertical edges
        0, 4, 1, 5, 2, 6, 3, 7
    };

    cubeGeometry.vao = std::make_unique<VAO>();
    cubeGeometry.vbo = std::make_unique<VBO>(vertices.size() * sizeof(glm::vec3), GL_STATIC_DRAW);
    cubeGeometry.ebo = std::make_unique<EBO>(indices);

    cubeGeometry.vao->Bind();
    cubeGeometry.vbo->UpdateData(vertices.data(), vertices.size() * sizeof(glm::vec3));

    cubeGeometry.ebo->Bind();

    // Position attribute
    cubeGeometry.vao->LinkAttrib(*cubeGeometry.vbo, 0, 3, GL_FLOAT, sizeof(glm::vec3), (void*)0);

    cubeGeometry.indexCount = static_cast<unsigned int>(indices.size());
    cubeGeometry.vao->Unbind();
}

void DebugDrawSystem::CreateSphereGeometry()
{
    std::vector<glm::vec3> vertices;
    std::vector<GLuint> indices;

    const int subdivisions = 2; // Adjust for more/less detail (0-4 recommended)

    // Start with icosahedron vertices
    const float t = (1.0f + sqrt(5.0f)) / 2.0f; // Golden ratio
    const float scale = 0.5f / sqrt(1.0f + t * t); // Normalize to radius 0.5

    // 12 vertices of icosahedron
    std::vector<glm::vec3> icosahedronVertices = {
        {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}
    };

    // Normalize vertices to sphere
    for (auto& vertex : icosahedronVertices) {
        vertex = glm::normalize(vertex) * scale;
    }

    // 20 triangular faces of icosahedron
    std::vector<std::array<int, 3>> icosahedronFaces = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    vertices = icosahedronVertices;

    // Subdivide faces
    for (int sub = 0; sub < subdivisions; ++sub) {
        std::vector<std::array<int, 3>> newFaces;
        std::map<std::pair<int, int>, int> midpointCache;

        auto getMidpoint = [&](int i1, int i2) -> int {
            auto key = std::make_pair(std::min(i1, i2), std::max(i1, i2));
            auto it = midpointCache.find(key);
            if (it != midpointCache.end()) {
                return it->second;
            }

            glm::vec3 midpoint = glm::normalize(vertices[i1] + vertices[i2]) * scale;
            vertices.push_back(midpoint);
            int index = static_cast<int>(vertices.size()) - 1;
            midpointCache[key] = index;
            return index;
            };

        for (const auto& face : icosahedronFaces) {
            int mid1 = getMidpoint(face[0], face[1]);
            int mid2 = getMidpoint(face[1], face[2]);
            int mid3 = getMidpoint(face[2], face[0]);

            newFaces.push_back({ face[0], mid1, mid3 });
            newFaces.push_back({ face[1], mid2, mid1 });
            newFaces.push_back({ face[2], mid3, mid2 });
            newFaces.push_back({ mid1, mid2, mid3 });
        }

        icosahedronFaces = newFaces;
    }

    // Create wireframe indices from faces
    std::set<std::pair<int, int>> edgeSet;
    for (const auto& face : icosahedronFaces) {
        for (int i = 0; i < 3; ++i) {
            int v1 = face[i];
            int v2 = face[(i + 1) % 3];
            edgeSet.insert({ std::min(v1, v2), std::max(v1, v2) });
        }
    }

    // Convert edges to line indices
    for (const auto& edge : edgeSet) {
        indices.push_back(edge.first);
        indices.push_back(edge.second);
    }

    sphereGeometry.vao = std::make_unique<VAO>();
    sphereGeometry.vbo = std::make_unique<VBO>(vertices.size() * sizeof(glm::vec3), GL_STATIC_DRAW);
    sphereGeometry.ebo = std::make_unique<EBO>(indices);

    sphereGeometry.vao->Bind();
    sphereGeometry.vbo->UpdateData(vertices.data(), vertices.size() * sizeof(glm::vec3));
    sphereGeometry.ebo->Bind();

    sphereGeometry.vao->LinkAttrib(*sphereGeometry.vbo, 0, 3, GL_FLOAT, sizeof(glm::vec3), (void*)0);

    sphereGeometry.indexCount = static_cast<unsigned int>(indices.size());
    sphereGeometry.vao->Unbind();
}

void DebugDrawSystem::CreateLineGeometry()
{
    std::vector<glm::vec3> vertices = 
    {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f}
    };

    lineGeometry.vao = std::make_unique<VAO>();
    lineGeometry.vbo = std::make_unique<VBO>(vertices.size() * sizeof(glm::vec3), GL_STATIC_DRAW);

    lineGeometry.vao->Bind();
    lineGeometry.vbo->UpdateData(vertices.data(), vertices.size() * sizeof(glm::vec3));
    lineGeometry.vao->LinkAttrib(*lineGeometry.vbo, 0, 3, GL_FLOAT, sizeof(glm::vec3), (void*)0);

    lineGeometry.indexCount = 2;
    lineGeometry.vao->Unbind();
}

void DebugDrawSystem::SubmitDebugRenderItems()
{
}

void DebugDrawSystem::UpdateTimedCommands(float deltaTime)
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities) 
    {
        auto& debugComponent = ecsManager.GetComponent<DebugDrawComponent>(entity);

        // Remove expired commands
        debugComponent.drawCommands.erase(
            std::remove_if(debugComponent.drawCommands.begin(), debugComponent.drawCommands.end(),
                [deltaTime](DebugDrawData& cmd) {
                    if (cmd.duration > 0.0f) {
                        cmd.duration -= deltaTime;
                        return cmd.duration <= 0.0f;
                    }
                    return false;
                }),
            debugComponent.drawCommands.end()
        );
    }
}

void DebugDrawSystem::DrawCube(const Vector3D& position, const Vector3D& scale, const Vector3D& color, float duration)
{
    DebugDrawData cubeData(DebugDrawType::CUBE);
    cubeData.position = position;
    cubeData.scale = scale;
    cubeData.color = color;
    cubeData.duration = duration;

    debugQueue.push_back(cubeData);
}

void DebugDrawSystem::DrawSphere(const Vector3D& position, float radius, const Vector3D& color, float duration)
{
    DebugDrawData sphereData(DebugDrawType::SPHERE);
    sphereData.position = position;
    sphereData.scale = Vector3D(radius * 2.0f, radius * 2.0f, radius * 2.0f); // Scale by diameter
    sphereData.color = color;
    sphereData.duration = duration;

    debugQueue.push_back(sphereData);
}

void DebugDrawSystem::DrawLine(const Vector3D& start, const Vector3D& end, const Vector3D& color, float duration, float width)
{
    DebugDrawData lineData(DebugDrawType::LINE);
    lineData.position = start;
    lineData.endPosition = end;
    lineData.color = color;
    lineData.duration = duration;
    lineData.lineWidth = width;

    debugQueue.push_back(lineData);
}

void DebugDrawSystem::DrawMeshWireframe(std::shared_ptr<Model> model, const Vector3D& position, const Vector3D& color, float duration)
{
    if (!model) return;

    DebugDrawData meshData(DebugDrawType::MESH_WIREFRAME);
    meshData.position = position;
    meshData.scale = Vector3D(1.0f, 1.0f, 1.0f);
    meshData.color = color;
    meshData.duration = duration;
    meshData.meshModel = model;

    debugQueue.push_back(meshData);
}

// ==================== LIGHT GIZMOS ====================

void DebugDrawSystem::DrawDirectionalLightGizmo(const Vector3D& position, const Vector3D& direction, const Vector3D& color, float duration)
{
    // Draw a small sphere at the center
    DrawSphere(position, 0.15f, color, duration);

    // Normalize direction
    Vector3D dir = direction.Normalized();

    // Draw main arrow showing light direction
    Vector3D arrowEnd = position + dir * 1.5f;
    DrawLine(position, arrowEnd, color, duration, 3.0f);

    // Draw arrowhead
    Vector3D perpendicular1 = Vector3D(-dir.y, dir.x, 0.0f).Normalized();
    if (perpendicular1.Length() < 0.01f) {
        perpendicular1 = Vector3D(1.0f, 0.0f, 0.0f);
    }
    Vector3D perpendicular2 = dir.Cross(perpendicular1).Normalized();

    float arrowSize = 0.3f;
    DrawLine(arrowEnd, arrowEnd - dir * arrowSize + perpendicular1 * arrowSize * 0.5f, color, duration, 2.0f);
    DrawLine(arrowEnd, arrowEnd - dir * arrowSize - perpendicular1 * arrowSize * 0.5f, color, duration, 2.0f);
    DrawLine(arrowEnd, arrowEnd - dir * arrowSize + perpendicular2 * arrowSize * 0.5f, color, duration, 2.0f);
    DrawLine(arrowEnd, arrowEnd - dir * arrowSize - perpendicular2 * arrowSize * 0.5f, color, duration, 2.0f);

    // Draw sun rays (8 rays around the sphere)
    for (int i = 0; i < 8; i++)
    {
        float angle = (i / 8.0f) * 2.0f * 3.14159265f;
        Vector3D rayStart = position + perpendicular1 * cos(angle) * 0.2f + perpendicular2 * sin(angle) * 0.2f;
        Vector3D rayEnd = position + perpendicular1 * cos(angle) * 0.5f + perpendicular2 * sin(angle) * 0.5f;
        DrawLine(rayStart, rayEnd, color, duration, 2.0f);
    }
}

void DebugDrawSystem::DrawPointLightGizmo(const Vector3D& position, float radius, const Vector3D& color, float duration)
{
    // Draw central sphere
    DrawSphere(position, 0.15f, color, duration);

    // Draw outer sphere showing light range
    DrawSphere(position, radius, color * 0.5f, duration);

    // Draw rays emanating from center (6 directions)
    float rayLength = radius * 0.7f;
    Vector3D directions[] = {
        Vector3D(1, 0, 0), Vector3D(-1, 0, 0),
        Vector3D(0, 1, 0), Vector3D(0, -1, 0),
        Vector3D(0, 0, 1), Vector3D(0, 0, -1)
    };

    for (int i = 0; i < 6; i++)
    {
        Vector3D rayEnd = position + directions[i] * rayLength;
        DrawLine(position, rayEnd, color, duration, 2.0f);

        // Draw small perpendicular lines at the end of each ray
        Vector3D perp;
        if (i < 2) perp = Vector3D(0, 1, 0); // For X-axis rays
        else if (i < 4) perp = Vector3D(1, 0, 0); // For Y-axis rays
        else perp = Vector3D(1, 0, 0); // For Z-axis rays

        float perpSize = 0.15f;
        DrawLine(rayEnd + perp * perpSize, rayEnd - perp * perpSize, color, duration, 1.5f);
    }
}

void DebugDrawSystem::DrawSpotLightGizmo(const Vector3D& position, const Vector3D& direction, float angle, float range, const Vector3D& color, float duration)
{
    // Draw small sphere at position
    DrawSphere(position, 0.15f, color, duration);

    // Normalize direction
    Vector3D dir = direction.Normalized();

    // Calculate cone radius at the end
    float coneRadius = range * tan(angle);

    // Create perpendicular vectors
    Vector3D perpendicular1 = Vector3D(-dir.y, dir.x, 0.0f).Normalized();
    if (perpendicular1.Length() < 0.01f) {
        perpendicular1 = Vector3D(1.0f, 0.0f, 0.0f);
    }
    Vector3D perpendicular2 = dir.Cross(perpendicular1).Normalized();

    // Draw cone outline (8 lines from apex to circle)
    Vector3D coneEnd = position + dir * range;
    int numLines = 8;
    for (int i = 0; i < numLines; i++)
    {
        float circleAngle = (i / (float)numLines) * 2.0f * 3.14159265f;
        Vector3D pointOnCircle = coneEnd +
            perpendicular1 * cos(circleAngle) * coneRadius +
            perpendicular2 * sin(circleAngle) * coneRadius;
        DrawLine(position, pointOnCircle, color, duration, 2.0f);
    }

    // Draw circle at the end of cone
    for (int i = 0; i < numLines; i++)
    {
        float angle1 = (i / (float)numLines) * 2.0f * 3.14159265f;
        float angle2 = ((i + 1) / (float)numLines) * 2.0f * 3.14159265f;

        Vector3D point1 = coneEnd +
            perpendicular1 * cos(angle1) * coneRadius +
            perpendicular2 * sin(angle1) * coneRadius;
        Vector3D point2 = coneEnd +
            perpendicular1 * cos(angle2) * coneRadius +
            perpendicular2 * sin(angle2) * coneRadius;

        DrawLine(point1, point2, color, duration, 1.5f);
    }

    // Draw center line showing direction
    DrawLine(position, coneEnd, color, duration, 2.5f);
}
