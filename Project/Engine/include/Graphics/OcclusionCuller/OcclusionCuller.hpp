#pragma once

// Hardware occlusion queries are a desktop-only feature.
// Android/GLES does not expose GL_ANY_SAMPLES_PASSED as a core feature.
#ifndef ANDROID

#include "Graphics/OpenGL.h"
#include "Graphics/Frustum/Frustum.hpp"
#include <glm/glm.hpp>
#include <unordered_map>
#include <cstdint>

/**
 * @brief Hardware occlusion culler using GL_ANY_SAMPLES_PASSED queries.
 *
 * Algorithm (two-phase, one-frame latency):
 *   Frame N render:
 *     Phase 1  - Render all entities whose last known result is VISIBLE.
 *     Phase 2  - With color + depth writes disabled, issue an AABB proxy
 *                query for EVERY frustum-visible entity (both visible and
 *                occluded lists). The queries test against the depth buffer
 *                that was populated during Phase 1.
 *   Frame N+1 BeginFrame:
 *     Non-blocking collection of all pending query results.
 *     Results drive Phase 1 of the next render.
 */
class OcclusionCuller
{
public:
    void Initialize();
    void Shutdown();

    /**
     * @brief Collect results from the previous frame's GL queries.
     *        Must be called once at the start of every frame before Render().
     */
    void BeginFrame();

    /**
     * @brief Returns true if the entity should be SKIPPED this frame.
     *        Entities with no prior data are treated as visible (returns false).
     */
    bool IsOccluded(uint32_t entityId) const;

    /**
     * @brief Issue an occlusion query for an entity's world-space AABB.
     *        The caller must disable color and depth writes before calling,
     *        then restore them afterwards.
     */
    void SubmitQuery(uint32_t entityId,
                     const AABB& worldBBox,
                     const glm::mat4& viewProjection);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled()        const { return m_enabled;  }

    int  GetOccludedCount() const { return m_occludedCount; }

private:
    struct EntityQuery
    {
        GLuint queryId       = 0;
        bool   pendingResult = false; // query submitted, not yet collected
        bool   lastVisible   = true;  // last known visibility (default: visible)
    };

    std::unordered_map<uint32_t, EntityQuery> m_entityData;

    // Proxy geometry: unit cube [-0.5, 0.5]^3, transformed to AABB in the shader.
    GLuint m_proxyVAO = 0;
    GLuint m_proxyVBO = 0;
    GLuint m_proxyEBO = 0;

    // Minimal GLSL program compiled from inline source strings.
    GLuint m_program         = 0;
    GLint  m_vpLoc           = -1;
    GLint  m_bboxMinLoc      = -1;
    GLint  m_bboxMaxLoc      = -1;

    bool m_enabled     = false;
    bool m_initialized = false;
    int  m_occludedCount = 0;

    void   InitProxyGeometry();
    GLuint CompileProgram();
    void   DrawAABBProxy(const AABB& bbox, const glm::mat4& viewProjection);
};

#endif // !ANDROID
