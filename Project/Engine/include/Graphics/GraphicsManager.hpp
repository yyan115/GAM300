#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "IRenderComponent.hpp"
#include "Graphics/LightManager.hpp"
#include "Graphics/Camera.h"
#include "Graphics/ShaderClass.h"
#include "Graphics/Model/Model.h"
#include "Model/ModelRenderComponent.hpp"
#include "TextRendering/Font.hpp"
#include "TextRendering/TextRenderComponent.hpp"
#include "DebugDraw/DebugDrawComponent.hpp"
#include <Math/Matrix4x4.hpp>

class GraphicsManager {
public:
	static GraphicsManager& GetInstance();

	// Initialization 
	bool Initialize(int window_width, int window_height);
	void Shutdown();

    // Frame management
    void BeginFrame();
    void EndFrame();
    void Clear(float r = 0.2f, float g = 0.3f, float b = 0.3f, float a = 1.0f);

    // Camera management
    void SetCamera(Camera* camera);
    Camera* GetCurrentCamera() const { return currentCamera; }

    // Render queue management
    void Submit(std::unique_ptr<IRenderComponent> renderItem);

    // Main rendering
    void Render();

private:
    GraphicsManager() = default;
    ~GraphicsManager() = default;

    GraphicsManager(const GraphicsManager&) = delete;
    GraphicsManager& operator=(const GraphicsManager&) = delete;

    // Private model rendering methods
    void RenderModel(const ModelRenderComponent& item);
    void ApplyLighting(Shader& shader);
    void SetupMatrices(Shader& shader, const glm::mat4& modelMatrix);
    
    glm::mat4 CreateTransformMatrix(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);

    // Private text rendering methods
    void RenderText(const TextRenderComponent& item);
    void Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scale);

    std::vector<std::unique_ptr<IRenderComponent>> renderQueue;
    Camera* currentCamera = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;

    // Debug Draw
    void RenderDebugDraw(const DebugDrawComponent& item);

    // Remove later
    glm::vec3 pointLightPositions[4] = {
        glm::vec3(0.7f,  0.2f,  2.0f),
        glm::vec3(2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f,  2.0f, -12.0f),
        glm::vec3(0.0f,  0.0f, -1.0f)
    };
};