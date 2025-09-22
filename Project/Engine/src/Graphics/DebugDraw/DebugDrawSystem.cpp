#include "pch.h"
#include "Graphics/DebugDraw/DebugDrawSystem.hpp"
#include "Graphics/DebugDraw/DebugDrawComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Transform/TransformComponent.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "Asset Manager/ResourceManager.hpp"

std::vector<DebugDrawData> DebugDrawSystem::debugQueue; 

bool DebugDrawSystem::Initialise() 
{
    // Create debug shader (you'll need to create this shader file)
    debugShader = ResourceManager::GetInstance().GetResource<Shader>("Resources/Shaders/debug");

    CreatePrimitiveGeometry();

    std::cout << "[DebugDrawSystem] Initialized" << std::endl;
    return true;
}

void DebugDrawSystem::Update()
{
    if (debugQueue.empty()) return;

    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Create component and check all pointers are valid
    auto debugComponent = std::make_unique<DebugDrawComponent>();
    auto shader = ResourceManager::GetInstance().GetResource<Shader>("Resources/Shaders/debug");

    if (!shader || !cubeGeometry.vao || !sphereGeometry.vao || !lineGeometry.vao) {
        std::cerr << "Error: Required debug resources are null!" << std::endl;
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
    std::cout << "[DebugDrawSystem] Shutdown" << std::endl;
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

    cubeGeometry.indexCount = indices.size();
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
            int index = vertices.size() - 1;
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

    sphereGeometry.indexCount = indices.size();
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

void DebugDrawSystem::DrawCube(const glm::vec3& position, const glm::vec3& scale, const glm::vec3& color, float duration)
{
    DebugDrawData cubeData(DebugDrawType::CUBE);
    cubeData.position = position;
    cubeData.scale = scale;
    cubeData.color = color;
    cubeData.duration = duration;

    debugQueue.push_back(cubeData);
}

void DebugDrawSystem::DrawSphere(const glm::vec3& position, float radius, const glm::vec3& color, float duration)
{
    DebugDrawData sphereData(DebugDrawType::SPHERE);
    sphereData.position = position;
    sphereData.scale = glm::vec3(radius * 2.0f); // Scale by diameter
    sphereData.color = color;
    sphereData.duration = duration;

    debugQueue.push_back(sphereData);
}

void DebugDrawSystem::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color, float duration, float width)
{
    DebugDrawData lineData(DebugDrawType::LINE);
    lineData.position = start;
    lineData.endPosition = end;
    lineData.color = color;
    lineData.duration = duration;
    lineData.lineWidth = width;

    debugQueue.push_back(lineData);
}

void DebugDrawSystem::DrawMeshWireframe(std::shared_ptr<Model> model, const glm::vec3& position, const glm::vec3& color, float duration)
{
    if (!model) return;

    DebugDrawData meshData(DebugDrawType::MESH_WIREFRAME); 
    meshData.position = position; 
    meshData.scale = glm::vec3(1.0f); 
    meshData.color = color; 
    meshData.duration = duration; 
    meshData.meshModel = model; 

    debugQueue.push_back(meshData); 
}
