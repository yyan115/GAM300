#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "IRenderComponent.hpp"
#include "Graphics/Camera/Camera.h"
#include "Graphics/ShaderClass.h"
#include "Graphics/Model/Model.h"
#include "Model/ModelRenderComponent.hpp"
#include "TextRendering/Font.hpp"
#include "TextRendering/TextRenderComponent.hpp"
#include "DebugDraw/DebugDrawComponent.hpp"
#include "Sprite/SpriteRenderComponent.hpp"
#include <Math/Matrix4x4.hpp>
#include "Engine.h"  // For ENGINE_API macro
#include "Particle/ParticleComponent.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Graphics/Frustum/Frustum.hpp"

struct ViewportDimensions {
    int width = 0;
    int height = 0;
    float aspectRatio = 1.0f;
};

struct CullingStats {
    int totalObjects = 0;
    int culledObjects = 0;

    float GetCulledPercentage() const {
        if (totalObjects == 0) return 0.0f;
        return (culledObjects * 100.0f) / totalObjects;
    }

    void Reset() {
        totalObjects = 0;
        culledObjects = 0;
    }
};

class GraphicsManager {
public:
	enum class ViewMode {
		VIEW_3D,      // 3D mode - show 3D models and 3D sprites
		VIEW_2D       // 2D mode - show 2D sprites only in screen space
	};

	ENGINE_API static GraphicsManager& GetInstance();

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

    // Viewport management (for editor/scene panel rendering with correct aspect ratio)
    void ENGINE_API SetViewportSize(int width, int height);
    void GetViewportSize(int& width, int& height) const;

    // View mode management (2D/3D toggle)
    void SetViewMode(ViewMode mode) { viewMode = mode; }
    ViewMode GetViewMode() const { return viewMode; }
    bool Is3DMode() const { return viewMode == ViewMode::VIEW_3D; }
    bool Is2DMode() const { return viewMode == ViewMode::VIEW_2D; }

    // Editor rendering flag (to distinguish editor from game rendering)
    void SetRenderingForEditor(bool isEditor) { isRenderingForEditor = isEditor; }
    bool IsRenderingForEditor() const { return isRenderingForEditor; }

    // Target game resolution for 2D rendering (used to sync Scene and Game panels)
    void SetTargetGameResolution(int width, int height) { targetGameWidth = width; targetGameHeight = height; }
    void GetTargetGameResolution(int& width, int& height) const { width = targetGameWidth; height = targetGameHeight; }

    // Render queue management
    void Submit(std::unique_ptr<IRenderComponent> renderItem);

    // Main rendering
    void Render();
    void RenderSkybox();

    // FRUSTUM CULLING FUNCTIONS:
    void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const { return frustumCullingEnabled; }
    const Frustum& GetFrustum() const { return viewFrustum; }
    void ENGINE_API UpdateFrustum(); // Update frustum based on current camera and viewport

private:
    GraphicsManager() = default;
    ~GraphicsManager() = default;

    GraphicsManager(const GraphicsManager&) = delete;
    GraphicsManager& operator=(const GraphicsManager&) = delete;

    // Private model rendering methods
    void RenderModel(const ModelRenderComponent& item);
    void SetupMatrices(Shader& shader, const glm::mat4& modelMatrix, bool includeNormalMatrix = false);
    
    glm::mat4 CreateTransformMatrix(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale);

    // Private text rendering methods
    void RenderText(const TextRenderComponent& item);
    void Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scaleX, float scaleY);

    std::vector<std::unique_ptr<IRenderComponent>> renderQueue;
    Camera* currentCamera = nullptr;
    int screenWidth = 0;
    int screenHeight = 0;

    // Viewport dimensions for proper aspect ratio (set by editor/scene panel)
    int viewportWidth = 0;
    int viewportHeight = 0;

    // View mode state (2D/3D toggle)
    ViewMode viewMode = ViewMode::VIEW_3D;

    // Flag to indicate if currently rendering for editor (vs game)
    bool isRenderingForEditor = false;

    // Target game resolution for 2D rendering synchronization
    int targetGameWidth = 1920;
    int targetGameHeight = 1080;

    // Debug Draw
    void RenderDebugDraw(const DebugDrawComponent& item);

    // Particle
    void RenderParticles(const ParticleComponent& item);

    // Sprite rendering methods
    void RenderSprite(const SpriteRenderComponent& item);
    void Setup2DSpriteMatrices(Shader& shader, const glm::vec3& position,
        const glm::vec3& scale, float rotation);
    void Setup3DSpriteMatrices(Shader& shader, const glm::mat4& modelMatrix);

    // FRUSTUM MEMBERS:
    Frustum viewFrustum;
    bool frustumCullingEnabled = true;
    ViewportDimensions currentFrameViewport;
    ViewportDimensions GetCurrentViewport() const;
    CullingStats cullingStats;

    // Skybox rendering
    unsigned int skyboxVAO = 0;
    unsigned int skyboxVBO = 0;
    std::shared_ptr<Shader> skyboxShader = nullptr;
    void InitializeSkybox();

    // Multi-threading mutex
    std::mutex renderQueueMutex;
};