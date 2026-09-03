#pragma once
#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include <memory>
#include <mutex>
#include <type_traits>
#include <glm/glm.hpp>
#include "IRenderComponent.hpp"
#include "Graphics/Camera/Camera.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Model/Model.h"
#include "Model/ModelRenderComponent.hpp"
#include "TextRendering/Font.hpp"
#include "TextRendering/TextRenderComponent.hpp"
#include "TextRendering/TextRenderItem.hpp"
#include "DebugDraw/DebugDrawComponent.hpp"
#include "Sprite/SpriteRenderComponent.hpp"
#include "Sprite/SpriteRenderItem.hpp"
#include <Math/Matrix4x4.hpp>
#include "Engine.h"  // For ENGINE_API macro
#include "Particle/ParticleComponent.hpp"
#include "Particle/ParticleRenderItem.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Graphics/Frustum/Frustum.hpp"
#include "RenderSorter.hpp"
#include "Fog/FogComponent.hpp"

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

	enum class CullMode {
		BACK,
		FRONT,
		FRONT_AND_BACK
	};

	enum class FrontFace {
		CCW,
		CW
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
    void Submit(IRenderComponent* renderItem);

    template <typename T>
    void SubmitBatch(std::vector<T>& renderItems, std::size_t count)
    {
        static_assert(std::is_base_of_v<IRenderComponent, T>);
        count = std::min(count, renderItems.size());
        if (count == 0) return;

        std::lock_guard<std::mutex> lock(renderQueueMutex);
        renderQueue.reserve(renderQueue.size() + count);
        for (std::size_t index = 0; index < count; ++index)
        {
            IRenderComponent* renderItem = &renderItems[index];
            if (renderItem->isVisible)
            {
                renderQueue.push_back(renderItem);
            }
        }
    }

    // Main rendering
    void Render();
    void RenderSkybox();

    // Deferred rendering (items excluded from post-processing, rendered on top)
    void RenderDeferred();
    bool HasDeferredItems() const { return !deferredQueue.empty(); }

    // Game panel active flag (prevents double deferred render)
    void SetGamePanelActive(bool active) { gamePanelActive = active; }
    bool IsGamePanelActive() const { return gamePanelActive; }

	// Face Culling
	void SetFaceCulling(bool enabled);
	bool IsFaceCullingEnabled() const { return faceCullingEnabled; }
	void SetCullMode(CullMode mode);
	CullMode GetCullMode() const { return cullMode; }
	void SetFrontFace(FrontFace face);
	FrontFace GetFrontFace() const { return frontFace; }

    // FRUSTUM CULLING FUNCTIONS:
    void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const { return frustumCullingEnabled; }
    const Frustum& GetFrustum() const { return viewFrustum; }
    void ENGINE_API UpdateFrustum(); // Update frustum based on current camera and viewport

    // Far plane distance (controls culling and rendering distance)
    ENGINE_API void SetFarPlane(float f) { m_farPlane = f; }
    float GetFarPlane() const { return m_farPlane; }

    // Depth prepass toggle (enabled by default on PC, disabled on Android)
    void SetDepthPrepassEnabled(bool enabled) { m_depthPrepassEnabled = enabled; }
    bool IsDepthPrepassEnabled() const { return m_depthPrepassEnabled; }

    // Per-light shadow culling: call before each point light shadow render
    void SetPointShadowCullData(const glm::vec3& lightPos, float farPlane) { m_shadowLightPos = lightPos; m_shadowFarPlane = farPlane; }
    void ClearPointShadowCullData() { m_shadowFarPlane = -1.0f; }

    const SortingStats& GetSortingStats() const { return m_sortingStats; }

    // Environment reflection state (read by instancing manager)
    bool IsEnvReflectionActive() const { return envReflectionActive; }
    float GetEnvReflectionIntensity() const { return envReflectionIntensityValue; }

    // Bloom dirty tracking - skip the whole bloom post-process pass when no
    // entity on screen has bloom emission this frame. Saves ~3-5ms on mobile.
    void ResetBloomFlag() noexcept {
        m_hasBloomEmissionThisFrame.store(false, std::memory_order_relaxed);
    }
    void NotifyBloomUsedThisFrame() noexcept {
        m_hasBloomEmissionThisFrame.store(true, std::memory_order_relaxed);
    }
    bool HasBloomEmissionThisFrame() const noexcept {
        return m_hasBloomEmissionThisFrame.load(std::memory_order_relaxed);
    }
    // Instanced rendering shares the scene framebuffer and uses this to avoid
    // writing the bloom attachment for batches with no emission.
    void SetBloomOutputEnabled(bool enabled);
    bool IsBloomTargetPrepared() const { return m_bloomTargetPrepared; }

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
    void RenderText(const TextRenderItem& item);
    void Setup2DTextMatrices(Shader& shader, const glm::vec3& position, float scaleX, float scaleY);

    // Frame snapshots are owned and reused by their submitting systems.
    // These pointers are valid until the next BeginFrame().
    std::vector<IRenderComponent*> renderQueue;
    std::vector<IRenderComponent*> deferredQueue; // Post-process excluded items
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

    // Flag to indicate game panel is active (editor handles deferred rendering)
    bool gamePanelActive = false;

    // Target game resolution for 2D rendering synchronization
    int targetGameWidth = 1920;
    int targetGameHeight = 1080;

    // Debug Draw
    void RenderDebugDraw(const DebugDrawComponent& item);

    // Particle
    void RenderParticles(const ParticleComponent& item);
    void RenderParticles(const ParticleRenderItem& item);
    void RenderParticleInstances(
        const IRenderComponent& renderState,
        const std::shared_ptr<Texture>& texture,
        const std::shared_ptr<Shader>& shader,
        VAO* vao,
        std::size_t particleCount,
        bool additiveBlending,
        const glm::vec4& startColor,
        const glm::vec4& endColor,
        float startSize,
        float endSize);

    // Sprite rendering methods
    void RenderSprite(const SpriteRenderComponent& item);
    void RenderSprite(const SpriteRenderItem& item);
    void Setup2DSpriteMatrices(Shader& shader, const glm::vec3& position,
        const glm::vec3& scale, float rotation);
    void Setup3DSpriteMatrices(Shader& shader, const glm::mat4& modelMatrix);
    const glm::mat4& GetScreenProjection();

    // The sprite/text/particle pass changes the same handful of states for
    // every draw. Cache them across adjacent items to avoid redundant GLES
    // driver calls, then restore the engine's canonical state once per pass.
    struct CachedRenderState {
        std::int8_t depthTest = -1;
        std::int8_t depthWrite = -1;
        std::int8_t blend = -1;
        std::int8_t cullFace = -1;
        GLenum blendSource = 0;
        GLenum blendDestination = 0;
        bool blendFunctionKnown = false;
    };
    CachedRenderState m_cachedRenderState;
    GLuint m_cachedTexture2DUnit0 = 0;
    bool m_texture2DUnit0Known = false;
    void InvalidateRenderStateCache() noexcept;
    void SetDepthTestCached(bool enabled);
    void SetDepthWriteCached(bool enabled);
    void SetBlendCached(bool enabled);
    void SetCullFaceCached(bool enabled);
    void SetBlendFunctionCached(GLenum source, GLenum destination);
    void BindTexture2DUnit0Cached(GLuint texture);
    void RestoreDefaultRenderState();

    // FRUSTUM MEMBERS:
    Frustum viewFrustum;
    bool frustumCullingEnabled = true;
    float m_farPlane = 50.0f;
    ViewportDimensions currentFrameViewport;
    ViewportDimensions GetCurrentViewport() const;
    CullingStats cullingStats;

	// Face Culling state
	bool faceCullingEnabled = true; // Default enabled
	CullMode cullMode = CullMode::BACK; // Default back face culling
	FrontFace frontFace = FrontFace::CCW; // Default Counter-Clockwise

    // Skybox rendering
    unsigned int skyboxVAO = 0;
    unsigned int skyboxVBO = 0;
    std::shared_ptr<Shader> skyboxShader = nullptr;
    void InitializeSkybox();

    // Multi-threading mutex
    std::mutex renderQueueMutex;

    void RenderSceneForShadows(Shader& depthShader);

    // Depth prepass
    std::shared_ptr<Shader> m_depthPrepassShader;
    bool m_depthPrepassEnabled = true;
    void RunDepthPrepass(const glm::mat4& view, const glm::mat4& projection);

    // Camera UBO — camera data uploaded once per frame (binding = 0).
    // viewProjection avoids repeating projection * view for every 3D vertex.
    struct CameraUBOData {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 cameraPos;
        float _pad = 0.0f; // matches std140 implicit padding after vec3
        glm::mat4 viewProjection;
    };
    GLuint m_cameraUBO = 0;
    CameraUBOData m_lastCameraUBOData{};
    bool m_hasLastCameraUBOData = false;
    void InitCameraUBO();
    void UploadCameraUBO(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos);

#ifdef ANDROID
    // Visible skin palettes are packed into one streaming UBO per frame. Each
    // affine bone uses three vec4 columns: xyz is the linear transform and w
    // carries one translation component. The implicit final row is 0,0,0,1,
    // reducing both UBO bandwidth and vertex-shader blending work by 25%.
    static constexpr std::size_t kBonePaletteMatrixCount = 100;
    static constexpr std::size_t kBonePaletteColumnsPerMatrix = 3;
    static constexpr std::size_t kBonePaletteBytes =
        kBonePaletteMatrixCount * kBonePaletteColumnsPerMatrix *
        sizeof(glm::vec4);
    GLuint m_bonePaletteUBO = 0;
    std::size_t m_bonePaletteStride = kBonePaletteBytes;
    std::size_t m_bonePaletteUploadSize = 0;
    std::size_t m_boundBonePaletteOffset =
        std::numeric_limits<std::size_t>::max();
    std::vector<std::uint8_t> m_bonePaletteScratch;
    void InitBonePaletteUBO();
    void PrepareBonePalettes();
    void BindBonePalette(const ModelRenderComponent& item);
#endif

    // Camera matrices are identical for nearly every draw in a pass. Keep the
    // frame values so legacy shaders and particles do not rebuild them per item.
    glm::mat4 m_frameView{1.0f};
    glm::mat4 m_frameProjection{1.0f};
    glm::vec3 m_frameCameraRight{1.0f, 0.0f, 0.0f};
    glm::vec3 m_frameCameraUp{0.0f, 1.0f, 0.0f};

    glm::mat4 m_screenProjection{1.0f};
    int m_screenProjectionWidth = -1;
    int m_screenProjectionHeight = -1;

    // Current point light shadow data for per-light culling
    // Set before each point shadow render, -1 farPlane means directional (no sphere cull)
    glm::vec3 m_shadowLightPos = glm::vec3(0.0f);
    float m_shadowFarPlane = -1.0f;

    // Per-frame render scratch. These hold non-owning pointers only during a
    // render pass; keeping capacity avoids allocator churn in the frame loop.
    std::vector<IRenderComponent*> m_modelRenderItems;
    std::vector<IRenderComponent*> m_otherRenderItems;
    std::vector<IRenderComponent*> m_deferredModelRenderItems;
    std::vector<IRenderComponent*> m_deferredOtherRenderItems;
    std::vector<glm::vec4> m_textVertexScratch;

    // State sorting support
    struct ModelSortEntry {
        IRenderComponent* item = nullptr;
        RenderLayer::Type layer = RenderLayer::Type::LAYER_OPAQUE;
        float distanceSq = 0.0f;
        int depthBucket = 0;
        bool bloomOutput = false;
        RenderSortKey stateKey;
    };
    std::vector<ModelSortEntry> m_modelSortEntries;
    ResourceIdCache m_idCache;
    SortingStats m_sortingStats;

    // Track current bound state to avoid redundant switches
    Shader* m_currentShader = nullptr;
    Material* m_currentMaterial = nullptr;

    // Environment reflection state (set per-frame from active camera)
    bool envReflectionActive = false;
    float envReflectionIntensityValue = 1.0f;

    // Bloom dirty flag - reset at start of each frame, set when any entity
    // submits a bloom emission > 0. Used to skip the post-process bloom pass
    // entirely when nothing is glowing this frame.
    std::atomic_bool m_hasBloomEmissionThisFrame{false};
    bool m_bloomTargetPrepared = false;
    bool m_bloomOutputEnabled = false;

    void RenderModelOptimized(const ModelRenderComponent& item);

    // Fog
    void RenderFogVolume(const FogVolumeComponent& item);
};
