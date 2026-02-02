#pragma once
#include "../OpenGL.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <cfloat>

class Shader;

class PointShadowMap {
public:
    PointShadowMap() = default;
    ~PointShadowMap() { Shutdown(); }

    bool Initialize(int resolution = 1024);
    void Shutdown();

    // Render shadow map for a point light
    void Render(const glm::vec3& lightPos, float farPlane, std::function<void(Shader&)> renderCallback);

    // Apply shadow uniforms to a shader
    void Apply(Shader& shader, int textureUnit, int shadowIndex);

    // Getters
    GLuint GetDepthCubemap() const { return depthCubemap; }
    GLuint GetFBO() const { return depthMapFBO; }
    bool IsInitialized() const { return initialized; }
    int GetResolution() const { return resolution; }

    // Shadow parameters
    float bias = 0.05f;

    // =========================================================================
    // SHADOW CACHING - New methods and state
    // =========================================================================

    // Check if this shadow map needs to be re-rendered
    // Call this BEFORE Render() to decide whether to skip the render pass
    bool NeedsUpdate(const glm::vec3& lightPos, float farPlane) const;

    // Mark the shadow map as updated (call after Render())
    void MarkUpdated(const glm::vec3& lightPos, float farPlane);

    // Call once per frame to track staleness
    void IncrementFrameCounter();

    // Force a re-render next frame (call when scene geometry changes significantly)
    void Invalidate();

    // Configuration for caching behavior
    struct CacheConfig {
        float positionThreshold = 0.05f;   // Min movement to trigger update (world units)
        float farPlaneThreshold = 0.1f;    // Min far plane change to trigger update
        int maxStaleFrames = 60;           // Force update after this many frames
        int updateInterval = 1;            // Minimum frames between updates (1 = every frame if needed)
    };

    CacheConfig cacheConfig;

    // Debug getters
    glm::vec3 GetCachedPosition() const { return m_cachedLightPos; }
    float GetCachedFarPlane() const { return m_cachedFarPlane; }
    int GetFramesSinceUpdate() const { return m_framesSinceUpdate; }
    bool IsForceDirty() const { return m_forceDirty; }

    // Stats for debugging/profiling
    struct CacheStats {
        int totalFrames = 0;
        int updatesPerformed = 0;
        int updatesSaved = 0;

        float GetHitRate() const {
            if (totalFrames == 0) return 0.0f;
            return static_cast<float>(updatesSaved) / totalFrames * 100.0f;
        }

        void Reset() {
            totalFrames = 0;
            updatesPerformed = 0;
            updatesSaved = 0;
        }
    };

    const CacheStats& GetCacheStats() const { return cacheStats; }
    void ResetCacheStats() { cacheStats.Reset(); }

private:
    std::vector<glm::mat4> GetLightSpaceMatrices(const glm::vec3& lightPos, float nearPlane, float farPlane);

    GLuint depthMapFBO = 0;
    GLuint depthCubemap = 0;
    int resolution = 1024;
    bool initialized = false;
    std::shared_ptr<Shader> depthShader;
    float currentFarPlane = 25.0f;

    // =========================================================================
    // SHADOW CACHING - Internal state
    // =========================================================================

    // Cached state from last render
    glm::vec3 m_cachedLightPos = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    float m_cachedFarPlane = -1.0f;

    // Frame tracking
    int m_framesSinceUpdate = 999;  // Start high to force initial render
    int m_framesSinceLastCheck = 0;
    bool m_forceDirty = true;       // Start dirty to ensure first render

    // Stats
    CacheStats cacheStats;
};