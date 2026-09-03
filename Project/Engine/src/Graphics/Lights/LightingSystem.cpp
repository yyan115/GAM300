#include "pch.h"
#include "Graphics/Lights/LightingSystem.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include <cstring>

bool LightingSystem::Initialise()
{
    //std::cout << "[LightingSystem] Initializing..." << std::endl;

#if !defined(ANDROID) && !defined(__ANDROID__)
    // Initialize directional shadow map
    if (!directionalShadowMap.Initialize(shadowMapResolution))
    {
        //std::cout << "[LightingSystem] Warning: Directional shadow map failed" << std::endl;
        shadowsEnabled = false;
    }

    pointShadowMaps.resize(MAX_POINT_LIGHT_SHADOWS);
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (!pointShadowMaps[i].Initialize(pointShadowMapResolution))
        {
            //std::cout << "[LightingSystem] Warning: Point shadow map " << i << " failed" << std::endl;
        }
        // Desktop updates every frame.
        pointShadowMaps[i].cacheConfig.updateInterval = 0;
        pointShadowMaps[i].cacheConfig.maxStaleFrames = 1;
        pointShadowMaps[i].SetPhaseOffset(0);
    }
#else
    // Mobile uses a shadow-free shader variant. Do not allocate unused depth
    // textures/FBOs or carry their samplers through the fragment shader.
    shadowsEnabled = false;
#endif

#ifdef __ANDROID__
    // Allocate the lighting UBO (binding = 1). CameraBlock owns binding = 0.
    InitLightingUBO();
#endif

    const std::size_t pointCapacity = static_cast<std::size_t>(MAX_POINT_LIGHTS);
    pointLightData.positions.reserve(pointCapacity);
    pointLightData.ambient.reserve(pointCapacity);
    pointLightData.diffuse.reserve(pointCapacity);
    pointLightData.specular.reserve(pointCapacity);
    pointLightData.constant.reserve(pointCapacity);
    pointLightData.linear.reserve(pointCapacity);
    pointLightData.quadratic.reserve(pointCapacity);
    pointLightData.intensity.reserve(pointCapacity);
    pointLightData.range.reserve(pointCapacity);
    pointLightData.shadowIndex.reserve(pointCapacity);

    const std::size_t spotCapacity = static_cast<std::size_t>(MAX_SPOT_LIGHTS);
    spotLightData.positions.reserve(spotCapacity);
    spotLightData.directions.reserve(spotCapacity);
    spotLightData.ambient.reserve(spotCapacity);
    spotLightData.diffuse.reserve(spotCapacity);
    spotLightData.specular.reserve(spotCapacity);
    spotLightData.constant.reserve(spotCapacity);
    spotLightData.linear.reserve(spotCapacity);
    spotLightData.quadratic.reserve(spotCapacity);
    spotLightData.cutOff.reserve(spotCapacity);
    spotLightData.outerCutOff.reserve(spotCapacity);
    spotLightData.intensity.reserve(spotCapacity);

    m_allPointLights.reserve(entities.size());
    m_shadowCandidates.reserve(
        static_cast<std::size_t>(MAX_VISIBLE_POINT_LIGHTS));

    //std::cout << "[LightingSystem] Initialized" << std::endl;
    return true;
}


void LightingSystem::Update()
{
    PROFILE_FUNCTION();
    CollectLightData();

#ifdef __ANDROID__
    // After CollectLightData has sorted/culled/packed the light lists, push
    // everything into the UBO in one glBufferSubData call. All subsequent draws
    // this frame read from the UBO with zero per-draw CPU overhead.
    UploadLightingUBO();
#endif
}

void LightingSystem::Shutdown()
{
    directionalShadowMap.Shutdown();

    for (auto& psm : pointShadowMaps)
    {
        psm.Shutdown();
    }
    pointShadowMaps.clear();

#ifdef __ANDROID__
    if (m_lightingUBO != 0) {
        glDeleteBuffers(1, &m_lightingUBO);
        m_lightingUBO = 0;
    }
    m_hasUploadedLightingData = false;
#endif

    //std::cout << "[LightingSystem] Shutdown" << std::endl;
}

void LightingSystem::SetPointShadowQuality(int quality)
{
    const int resolutions[] = { 128, 256, 512 };
    int newRes = resolutions[std::clamp(quality, 0, 2)];

#if defined(ANDROID) || defined(__ANDROID__)
    // Android's renderer intentionally has no shadow targets.
    pointShadowMapResolution = newRes;
    return;
#endif

    if (newRes == pointShadowMapResolution) return;

    pointShadowMapResolution = newRes;

    for (auto& psm : pointShadowMaps)
        psm.Shutdown();

    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (!pointShadowMaps[i].Initialize(pointShadowMapResolution))
            //std::cout << "[LightingSystem] Warning: Point shadow map " << i << " failed at res " << newRes << std::endl;


        pointShadowMaps[i].cacheConfig.updateInterval = 0;
        pointShadowMaps[i].cacheConfig.maxStaleFrames = 1;
        pointShadowMaps[i].SetPhaseOffset(0);
    }
}

void LightingSystem::ResetDefaults()
{
    ambientMode = AmbientMode::Color;
    ambientSky = glm::vec3(0.05f, 0.05f, 0.05f);
    ambientEquator = glm::vec3(0.03f, 0.03f, 0.03f);
    ambientGround = glm::vec3(0.01f, 0.01f, 0.01f);
    ambientIntensity = 1.0f;
}

void LightingSystem::RenderShadowMaps(unsigned int restoreFramebuffer, int restoreViewportWidth, int restoreViewportHeight)
{
    PROFILE_FUNCTION();

    if (!shadowsEnabled || !shadowRenderCallback)
    {
        return;
    }

    // Render directional shadow
    if (directionalLightData.hasDirectionalLight)
    {
        PROFILE_SCOPED("Shadow::Directional");
        Camera* camera = GraphicsManager::GetInstance().GetCurrentCamera();
        glm::vec3 sceneCenter = camera ? camera->Position : glm::vec3(0.0f);

        directionalShadowMap.Render(
            directionalLightData.direction,
            sceneCenter,
            shadowDistance,
            shadowRenderCallback
        );
    }
    else
    {
        // No directional light - clear stale shadow data so deleted lights don't leave ghost shadows
        directionalShadowMap.Clear();
    }

    // =========================================================================
    // POINT LIGHT SHADOWS WITH CACHING
    // =========================================================================

    // Increment frame counters for ALL shadow maps
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        pointShadowMaps[i].IncrementFrameCounter();
    }

    // LOGGING: Track updates this frame
    int updatedCount = 0;
    int skippedCount = 0;

    // Render only shadows that need updating
    int shadowIndex = 0;
    for (size_t i = 0; i < pointLightData.positions.size() && shadowIndex < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (pointLightData.shadowIndex[i] >= 0)
        {
            glm::vec3 lightPos = pointLightData.positions[i];

            float lightRange = pointLightData.range[i];
            if (pointShadowMaps[shadowIndex].NeedsUpdate(lightPos, lightRange))
            {
                PROFILE_SCOPED("Shadow::PointLight");
                GraphicsManager::GetInstance().SetPointShadowCullData(lightPos, lightRange);
                pointShadowMaps[shadowIndex].Render(lightPos, lightRange, shadowRenderCallback);
                GraphicsManager::GetInstance().ClearPointShadowCullData();
                pointShadowMaps[shadowIndex].MarkUpdated(lightPos, lightRange);
                updatedCount++;
            }
            else
            {
                skippedCount++;
            }

            shadowIndex++;
        }
    }

    // Shadow passes deliberately leave their framebuffer bound. Restore the
    // known HDR target once here instead of forcing synchronous GL state reads
    // inside every directional and point-light pass.
    glBindFramebuffer(GL_FRAMEBUFFER, restoreFramebuffer);
    glViewport(0, 0, restoreViewportWidth, restoreViewportHeight);
}

void LightingSystem::ApplyLighting(Shader& shader)
{
#ifdef __ANDROID__
    // Built-in Android shaders use the LightingBlock populated once per frame.
    // The vertex stage still needs this compact summary to skip normal/tangent
    // work on draws that have no normal-dependent lighting. These values change
    // only with the selected light set and Shader caches unchanged uniforms.
    if (shader.UsesLightingBlock()) {
        const int numPoint = std::min(
            static_cast<int>(pointLightData.positions.size()),
            LIGHTING_UBO_MAX_POINT_LIGHTS);
        const int numSpot = std::min(
            static_cast<int>(spotLightData.positions.size()),
            LIGHTING_UBO_MAX_SPOT_LIGHTS);
        const std::uint32_t pointMask = numPoint > 0
            ? (1u << static_cast<std::uint32_t>(numPoint)) - 1u
            : 0u;
        const std::uint32_t spotMask = numSpot > 0
            ? ((1u << static_cast<std::uint32_t>(numSpot)) - 1u)
                << LIGHTING_UBO_MAX_POINT_LIGHTS
            : 0u;
        const bool needsGlobalNormal =
            directionalLightData.hasDirectionalLight ||
            (ambientIntensity != 0.0f && ambientMode == AmbientMode::Gradient);
        shader.setInt(
            "u_vertexActiveLightMask",
            static_cast<int>(pointMask | spotMask));
        shader.setBool("u_vertexNeedsGlobalNormal", needsGlobalNormal);
        return;
    }
    // Keep the legacy uniform path available for custom shaders without that block.
#endif

    shader.setInt("ambientMode", static_cast<int>(ambientMode));
    shader.setVec3("ambientSky", ambientSky);
    shader.setVec3("ambientEquator", ambientEquator);
    shader.setVec3("ambientGround", ambientGround);
    shader.setFloat("ambientIntensity", ambientIntensity);

    // Apply directional light
    if (directionalLightData.hasDirectionalLight)
    {
        shader.setVec3("dirLight.direction", directionalLightData.direction);
        shader.setVec3("dirLight.ambient", directionalLightData.ambient);
        shader.setVec3("dirLight.diffuse", directionalLightData.diffuse);
        shader.setVec3("dirLight.specular", directionalLightData.specular);
        shader.setFloat("dirLight.intensity", directionalLightData.intensity);
    }
    else
    {
        shader.setVec3("dirLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
        shader.setVec3("dirLight.ambient", glm::vec3(0.0f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("dirLight.specular", glm::vec3(0.0f));
        shader.setFloat("dirLight.intensity", 0.0f);
    }

    // Send counts to shader
    shader.setInt("numPointLights", static_cast<int>(pointLightData.positions.size()));
    shader.setInt("numSpotLights", static_cast<int>(spotLightData.positions.size()));

    // Set active point lights
    for (size_t i = 0; i < pointLightData.positions.size(); i++)
    {
        std::string base = "pointLights[" + std::to_string(i) + "]";
        shader.setVec3(base + ".position", pointLightData.positions[i]);
        shader.setVec3(base + ".ambient", pointLightData.ambient[i]);
        shader.setVec3(base + ".diffuse", pointLightData.diffuse[i]);
        shader.setVec3(base + ".specular", pointLightData.specular[i]);
        shader.setFloat(base + ".constant", pointLightData.constant[i]);
        shader.setFloat(base + ".linear", pointLightData.linear[i]);
        shader.setFloat(base + ".quadratic", pointLightData.quadratic[i]);
        shader.setFloat(base + ".intensity", pointLightData.intensity[i]);
        shader.setFloat(base + ".range", pointLightData.range[i]);
    }

    // Set active spot lights
    for (size_t i = 0; i < spotLightData.positions.size(); i++)
    {
        std::string base = "spotLights[" + std::to_string(i) + "]";
        shader.setVec3(base + ".position", spotLightData.positions[i]);
        shader.setVec3(base + ".direction", spotLightData.directions[i]);
        shader.setVec3(base + ".ambient", spotLightData.ambient[i]);
        shader.setVec3(base + ".diffuse", spotLightData.diffuse[i]);
        shader.setVec3(base + ".specular", spotLightData.specular[i]);
        shader.setFloat(base + ".constant", spotLightData.constant[i]);
        shader.setFloat(base + ".linear", spotLightData.linear[i]);
        shader.setFloat(base + ".quadratic", spotLightData.quadratic[i]);
        shader.setFloat(base + ".cutOff", spotLightData.cutOff[i]);
        shader.setFloat(base + ".outerCutOff", spotLightData.outerCutOff[i]);
        shader.setFloat(base + ".intensity", spotLightData.intensity[i]);
    }
}

void LightingSystem::ApplyShadows(Shader& shader)
{
#if defined(ANDROID) || defined(__ANDROID__)
    // The mobile shader has no shadow uniforms or samplers.
    (void)shader;
    return;
#else

    // CRITICAL: Always set samplerCube uniforms to their dedicated texture units (9-12)
    // to prevent conflict with sampler2D textures at units 0-7.
    // If these aren't set, they default to 0 which causes "samplers of different type
    // assigned to same texture unit" error on OpenGL ES.
    static const std::string shadowMapNames[MAX_POINT_LIGHT_SHADOWS] = {
        "pointShadowMaps[0]", "pointShadowMaps[1]",
        "pointShadowMaps[2]", "pointShadowMaps[3]"
    };
    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        shader.setInt(shadowMapNames[i], 9 + i);
    }

    if (!shadowsEnabled)
    {
        shader.setBool("shadowsEnabled", false);
        return;
    }

    shader.setBool("shadowsEnabled", true);

    // Directional shadow
    if (directionalLightData.hasDirectionalLight)
    {
        directionalShadowMap.Apply(shader, 8);
    }

    // Point light shadows
    shader.setFloat("pointShadowFarPlane", pointLightShadowFarPlane);

    for (int i = 0; i < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        if (i < pointShadowMaps.size() && pointShadowMaps[i].IsInitialized())
        {
            pointShadowMaps[i].Apply(shader, 9 + i, i);  // Texture units 9, 10, 11, 12
        }
    }

    // Send shadow indices for each point light
    for (size_t i = 0; i < pointLightData.shadowIndex.size(); ++i)
    {
        shader.setInt("pointLights[" + std::to_string(i) + "].shadowIndex", pointLightData.shadowIndex[i]);
    }
#endif
}

void LightingSystem::CollectLightData()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    // Clear previous frame data
    pointLightData.positions.clear();
    pointLightData.ambient.clear();
    pointLightData.diffuse.clear();
    pointLightData.specular.clear();
    pointLightData.constant.clear();
    pointLightData.linear.clear();
    pointLightData.quadratic.clear();
    pointLightData.intensity.clear();
    pointLightData.range.clear();
    pointLightData.shadowIndex.clear();

    directionalLightData.hasDirectionalLight = false;

    spotLightData.positions.clear();
    spotLightData.directions.clear();
    spotLightData.ambient.clear();
    spotLightData.diffuse.clear();
    spotLightData.specular.clear();
    spotLightData.constant.clear();
    spotLightData.linear.clear();
    spotLightData.quadratic.clear();
    spotLightData.cutOff.clear();
    spotLightData.outerCutOff.clear();
    spotLightData.intensity.clear();

    // =========================================================================
    // GET CAMERA POSITION FOR DISTANCE CULLING
    // =========================================================================
    Camera* camera = GraphicsManager::GetInstance().GetCurrentCamera();
    glm::vec3 camPos = camera ? camera->Position : glm::vec3(0.0f);
    const GraphicsManager& graphics = GraphicsManager::GetInstance();
    const bool cullFiniteLights =
        camera && graphics.IsFrustumCullingEnabled();
    const Frustum& cameraFrustum = graphics.GetFrustum();

    // =========================================================================
    // REUSE CLASS-LEVEL VECTORS (avoids per-frame heap allocation)
    // =========================================================================
    m_allPointLights.clear();

    for (const auto& entity : entities)
    {
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        const Transform* transform = nullptr;
        if (auto transformComponent = ecsManager.TryGetComponent<Transform>(entity)) {
            transform = &transformComponent->get();
        }

        // Collect directional light (first one only)
        if (auto directionalComponent =
                ecsManager.TryGetComponent<DirectionalLightComponent>(entity))
        {
            auto& light = directionalComponent->get();
            if (light.enabled && !directionalLightData.hasDirectionalLight)
            {
                directionalLightData.hasDirectionalLight = true;

                glm::vec3 baseDirection = light.direction.ConvertToGLM();
                glm::vec3 direction = baseDirection;
                if (transform)
                {
                    direction = glm::normalize(
                        transform->worldRotation.RotateVector(light.direction).ConvertToGLM());
                }
                else
                {
                    direction = glm::normalize(baseDirection);
                }

                directionalLightData.direction = direction;
                directionalLightData.ambient = light.ambient.ConvertToGLM();
                directionalLightData.diffuse = light.diffuse.ConvertToGLM();
                directionalLightData.specular = light.specular.ConvertToGLM();
                directionalLightData.intensity = light.intensity;
            }
        }

        // =====================================================================
        // COLLECT ALL POINT LIGHTS INTO TEMPORARY VECTOR
        // =====================================================================
        if (auto pointComponent =
                ecsManager.TryGetComponent<PointLightComponent>(entity))
        {
            auto& light = pointComponent->get();

            if (light.enabled)
            {
                glm::vec3 position(0.0f);
                if (transform)
                {
                    position = transform->worldPosition.ConvertToGLM();
                }

                // The shader already treats range as a hard finite influence.
                // If that sphere cannot touch the camera frustum, omitting the
                // light is exact and shortens every visible fragment's PBR loop.
                if (cullFiniteLights && light.range > 0.0f &&
                    !cameraFrustum.IsSphereVisible(position, light.range)) {
                    continue;
                }

                const glm::vec3 cameraOffset = position - camPos;
                const float distanceSq = glm::dot(cameraOffset, cameraOffset);

                m_allPointLights.push_back({
                    position,
                    light.ambient.ConvertToGLM(),
                    light.diffuse.ConvertToGLM(),
                    light.specular.ConvertToGLM(),
                    light.constant,
                    light.linear,
                    light.quadratic,
                    light.intensity,
                    light.range,
                    light.castShadows,
                    distanceSq
                    });
            }
        }

        // Collect spot lights (unchanged)
        if (auto spotComponent =
                ecsManager.TryGetComponent<SpotLightComponent>(entity))
        {
            auto& light = spotComponent->get();

            if (light.enabled)
            {
                if (spotLightData.positions.size() < MAX_SPOT_LIGHTS)
                {
                    glm::vec3 position(0.0f);
                    glm::vec3 direction(0.0f, 0.0f, -1.0f);

                    if (transform)
                    {
                        position = transform->worldPosition.ConvertToGLM();
                        direction = glm::normalize(
                            transform->worldRotation
                                .RotateVector(Vector3D(0.0f, 0.0f, -1.0f))
                                .ConvertToGLM());
                    }

                    spotLightData.positions.push_back(position);
                    spotLightData.directions.push_back(direction);
                    spotLightData.ambient.push_back(light.ambient.ConvertToGLM());
                    spotLightData.diffuse.push_back(light.diffuse.ConvertToGLM());
                    spotLightData.specular.push_back(light.specular.ConvertToGLM());
                    spotLightData.constant.push_back(light.constant);
                    spotLightData.linear.push_back(light.linear);
                    spotLightData.quadratic.push_back(light.quadratic);
                    spotLightData.cutOff.push_back(light.cutOff);
                    spotLightData.outerCutOff.push_back(light.outerCutOff);
                    spotLightData.intensity.push_back(light.intensity);
                }
                else
                {
                    static bool spotLightWarningShown = false;
                    if (!spotLightWarningShown) {
                        //std::cout << "[LightingSystem] Warning: Maximum spot lights (" << MAX_SPOT_LIGHTS
                        //    << ") reached. Additional spot lights will be ignored." << std::endl;
                        spotLightWarningShown = true;
                    }
                }
            }
        }
    }

    // =========================================================================
    // POINT LIGHT DISTANCE CULLING
    // Sort all point lights by distance, keep only closest MAX_VISIBLE_POINT_LIGHTS
    // =========================================================================

    // Determine how many lights to keep
    size_t numLightsToKeep = std::min(m_allPointLights.size(), static_cast<size_t>(MAX_VISIBLE_POINT_LIGHTS));

    // Also respect the shader's maximum
    numLightsToKeep = std::min(numLightsToKeep, static_cast<size_t>(MAX_POINT_LIGHTS));

    // Only order the nearest lights that can reach the shader. Squared distance
    // preserves ordering and avoids a square root for every point light.
    if (numLightsToKeep < m_allPointLights.size()) {
        std::partial_sort(
            m_allPointLights.begin(),
            m_allPointLights.begin() + static_cast<std::ptrdiff_t>(numLightsToKeep),
            m_allPointLights.end(),
            [](const PointLightCandidate& a, const PointLightCandidate& b) {
                return a.distanceSqToCamera < b.distanceSqToCamera;
            });
    }
    else {
        std::sort(m_allPointLights.begin(), m_allPointLights.end(),
            [](const PointLightCandidate& a, const PointLightCandidate& b) {
                return a.distanceSqToCamera < b.distanceSqToCamera;
            });
    }

    // =========================================================================
    // BUILD FINAL POINT LIGHT ARRAYS (only closest N lights)
    // =========================================================================

#if !defined(ANDROID) && !defined(__ANDROID__)
    m_shadowCandidates.clear();
#endif

    for (size_t i = 0; i < numLightsToKeep; ++i)
    {
        const auto& light = m_allPointLights[i];

        pointLightData.positions.push_back(light.position);
        pointLightData.ambient.push_back(light.ambient);
        pointLightData.diffuse.push_back(light.diffuse);
        pointLightData.specular.push_back(light.specular);
        pointLightData.constant.push_back(light.constant);
        pointLightData.linear.push_back(light.linear);
        pointLightData.quadratic.push_back(light.quadratic);
        pointLightData.intensity.push_back(light.intensity);
        pointLightData.range.push_back(light.range);
        pointLightData.shadowIndex.push_back(-1);  // Will be assigned below

#if !defined(ANDROID) && !defined(__ANDROID__)
        // Track shadow candidates on platforms that render shadow maps.
        if (light.castShadows)
        {
            m_shadowCandidates.push_back({
                pointLightData.positions.size() - 1,
                light.distanceSqToCamera
                });
        }
#endif
    }

#if !defined(ANDROID) && !defined(__ANDROID__)
    // =========================================================================
    // SHADOW DISTANCE CULLING
    // From visible lights, assign shadows to closest MAX_POINT_LIGHT_SHADOWS
    // =========================================================================

    int pointShadowCount = 0;
    for (size_t i = 0; i < m_shadowCandidates.size() && pointShadowCount < MAX_POINT_LIGHT_SHADOWS; ++i)
    {
        size_t lightIndex = m_shadowCandidates[i].lightIndex;
        pointLightData.shadowIndex[lightIndex] = pointShadowCount;
        pointShadowCount++;
    }

    // Update active shadow caster count for editor
    activeShadowCasterCount = pointShadowCount;
#else
    activeShadowCasterCount = 0;
#endif

    // =========================================================================
    // DEBUG LOGGING (comment out in production)
    // =========================================================================
    /*static int debugFrameCounter = 0;
    debugFrameCounter++;
    if (debugFrameCounter % 60 == 0)
    {
        if (m_allPointLights.size() > numLightsToKeep)
        {
            std::cout << "[Light Culling] " << m_allPointLights.size() << " point lights in scene, "
                      << numLightsToKeep << " visible (culled "
                      << (m_allPointLights.size() - numLightsToKeep) << ")" << std::endl;
        }
    
        if (!m_shadowCandidates.empty())
        {
            std::cout << "[Shadow Culling] " << m_shadowCandidates.size() << " lights want shadows, "
                      << pointShadowCount << " assigned" << std::endl;
        }
    }*/
}

#ifdef __ANDROID__
// ============================================================================
// Lighting UBO — allocates a buffer, binds to binding point 1 (CameraBlock = 0)
// ============================================================================
void LightingSystem::InitLightingUBO()
{
    if (m_lightingUBO != 0) return;

    glGenBuffers(1, &m_lightingUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_lightingUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightingUBOData), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_lightingUBO);  // binding = 1
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    m_hasUploadedLightingData = false;
}

// ============================================================================
// Pack current lighting state into the UBO and upload in one call.
// Called once per frame from Update() after CollectLightData().
// ============================================================================
void LightingSystem::UploadLightingUBO()
{
    if (m_lightingUBO == 0) return;

    LightingUBOData data{};

    // ---- Ambient (globals) ----
    data.ambSkyIntensity   = glm::vec4(ambientSky,     ambientIntensity);
    data.ambEquatorMode    = glm::vec4(ambientEquator, 0.0f);
    data.ambGround         = glm::vec4(ambientGround,  0.0f);

    // ---- Directional light ----
    if (directionalLightData.hasDirectionalLight) {
        data.dirLightDir = glm::vec4(
            directionalLightData.direction, directionalLightData.intensity);
        data.dirLightDiffuse = glm::vec4(
            directionalLightData.diffuse, 1.0f);
    } else {
        data.dirLightDir = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        data.dirLightDiffuse = glm::vec4(0.0f);
    }

    // ---- Light counts ----
    const int numPoint = std::min(static_cast<int>(pointLightData.positions.size()),
                                  LIGHTING_UBO_MAX_POINT_LIGHTS);
    const int numSpot = std::min(static_cast<int>(spotLightData.positions.size()),
                                 LIGHTING_UBO_MAX_SPOT_LIGHTS);
    const std::uint32_t pointMask = numPoint > 0
        ? (1u << static_cast<std::uint32_t>(numPoint)) - 1u
        : 0u;
    const std::uint32_t spotMask = numSpot > 0
        ? ((1u << static_cast<std::uint32_t>(numSpot)) - 1u)
            << LIGHTING_UBO_MAX_POINT_LIGHTS
        : 0u;
    // A zero ambient multiplier makes the entire ambient/AO path a no-op.
    // Encode that once on the CPU so fragments can skip it exactly. Negative
    // values remain active to preserve the existing behavior.
    const int packedAmbientMode = ambientIntensity == 0.0f
        ? -1
        : static_cast<int>(ambientMode);
    data.lightCounts = glm::ivec4(
        numPoint,
        numSpot,
        static_cast<int>(pointMask | spotMask),
        packedAmbientMode);

    // ---- Point lights (3 vec4s each) ----
    for (int i = 0; i < numPoint; ++i) {
        const int base = i * 3;
        const float range = pointLightData.range[i];
        const float invRange = range > 0.0f ? 1.0f / range : 0.0f;
        data.pointLights[base + 0] =
            glm::vec4(pointLightData.positions[i], invRange);
        data.pointLights[base + 1] =
            glm::vec4(pointLightData.diffuse[i], pointLightData.linear[i]);
        data.pointLights[base + 2] = glm::vec4(
            pointLightData.constant[i],
            pointLightData.quadratic[i],
            pointLightData.intensity[i],
            0.0f);
    }

    // ---- Spot lights (4 vec4s each) ----
    for (int i = 0; i < numSpot; ++i) {
        const int base = i * 4;
        const float cutoffWidth = std::max(
            spotLightData.cutOff[i] - spotLightData.outerCutOff[i], 0.0001f);
        data.spotLights[base + 0] =
            glm::vec4(spotLightData.positions[i], spotLightData.cutOff[i]);
        data.spotLights[base + 1] =
            glm::vec4(spotLightData.directions[i], spotLightData.outerCutOff[i]);
        data.spotLights[base + 2] =
            glm::vec4(spotLightData.diffuse[i], spotLightData.linear[i]);
        data.spotLights[base + 3] = glm::vec4(
            spotLightData.constant[i],
            spotLightData.quadratic[i],
            spotLightData.intensity[i],
            1.0f / cutoffWidth);
    }

    // Static scenes normally keep the same selected lights for many frames.
    // Avoid entering the driver when the packed UBO contents are unchanged.
    if (m_hasUploadedLightingData &&
        std::memcmp(&m_lastUploadedLightingData, &data, sizeof(data)) == 0) {
        return;
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_lightingUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightingUBOData), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    std::memcpy(&m_lastUploadedLightingData, &data, sizeof(data));
    m_hasUploadedLightingData = true;
    ++m_lightingRevision;
}

std::uint32_t LightingSystem::GetLightMask(
    const AABB& worldBounds) const noexcept
{
    std::uint32_t mask = 0;
    const std::size_t pointLightCount = std::min(
        pointLightData.positions.size(),
        static_cast<std::size_t>(LIGHTING_UBO_MAX_POINT_LIGHTS));

    for (std::size_t index = 0; index < pointLightCount; ++index) {
        const float range = pointLightData.range[index];
        if (range <= 0.0f) {
            mask |= (1u << index);
            continue;
        }

        const glm::vec3 closest = glm::clamp(
            pointLightData.positions[index], worldBounds.min, worldBounds.max);
        const glm::vec3 offset =
            pointLightData.positions[index] - closest;
        const float distanceSq = glm::dot(offset, offset);

        // Expand very slightly so CPU/GPU rounding at the hard range boundary
        // can only retain an extra light, never remove a visible contribution.
        const float conservativeRange =
            range + std::max(0.01f, range * 0.0001f);
        if (distanceSq <= conservativeRange * conservativeRange) {
            mask |= (1u << index);
        }
    }

    // A spotlight is exactly zero outside its outer cone. Test the AABB's
    // enclosing sphere against that infinite cone; rejecting the sphere also
    // rejects the box while retaining every potentially contributing light.
    const glm::vec3 center = worldBounds.GetCenter();
    const float radius = glm::length(worldBounds.GetExtents());
    const float conservativeRadius =
        radius + std::max(0.01f, radius * 0.0001f);
    const std::size_t spotLightCount = std::min(
        spotLightData.positions.size(),
        static_cast<std::size_t>(LIGHTING_UBO_MAX_SPOT_LIGHTS));

    for (std::size_t index = 0; index < spotLightCount; ++index) {
        const std::uint32_t bit =
            1u << (LIGHTING_UBO_MAX_POINT_LIGHTS + index);
        const float outerCos = spotLightData.outerCutOff[index];
        const glm::vec3 direction = spotLightData.directions[index];
        const float directionLengthSq = glm::dot(direction, direction);

        // Malformed cone data must retain the light rather than risk a visible
        // false-negative. Authored directions are normalized during collection.
        if (!(outerCos > 0.0f && outerCos <= 1.0f) ||
            !std::isfinite(outerCos) ||
            !std::isfinite(directionLengthSq) ||
            glm::abs(directionLengthSq - 1.0f) > 0.001f) {
            mask |= bit;
            continue;
        }

        const glm::vec3 fromApex =
            center - spotLightData.positions[index];
        const float axial = glm::dot(fromApex, direction);
        const float distanceSq = glm::dot(fromApex, fromApex);
        const float radial = std::sqrt(std::max(
            0.0f, distanceSq - axial * axial));
        const float outerSin =
            std::sqrt(std::max(0.0f, 1.0f - outerCos * outerCos));

        // In a 2D axial slice, the cone boundary is a ray. If the center
        // projects behind that ray, the apex is its nearest cone point;
        // otherwise use perpendicular distance to the boundary.
        const float boundaryProjection =
            axial * outerCos + radial * outerSin;
        const float distanceToCone = boundaryProjection <= 0.0f
            ? std::sqrt(std::max(distanceSq, 0.0f))
            : std::max(0.0f, radial * outerCos - axial * outerSin);

        if (distanceToCone <= conservativeRadius) {
            mask |= bit;
        }
    }

    return mask;
}
#endif
