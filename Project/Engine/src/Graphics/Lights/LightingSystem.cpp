#include "pch.h"
#include "Graphics/Lights/LightingSystem.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Asset Manager/ResourceManager.hpp"

bool LightingSystem::Initialise()
{
    std::cout << "[LightingSystem] Initializing..." << std::endl;

    // Initialize shadow mapping
    if (!InitializeShadowMaps())
    {
        std::cout << "[LightingSystem] Warning: Shadow map initialization failed, shadows disabled" << std::endl;
        shadowsEnabled = false;
    }

    std::cout << "[LightingSystem] Initialized" << std::endl;
    return true;
}

bool LightingSystem::InitializeShadowMaps()
{
    // Initialize directional shadow map
    if (!directionalShadowMap.Initialize(shadowMapResolution))
    {
        std::cout << "[LightingSystem] Failed to initialize directional shadow map" << std::endl;
        return false;
    }

    // Load shadow depth shader
    std::string shaderPath = ResourceManager::GetPlatformShaderPath("shadow_depth");
    shadowDepthShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

    if (!shadowDepthShader)
    {
        std::cout << "[LightingSystem] Failed to load shadow depth shader from: " << shaderPath << std::endl;
        return false;
    }

    std::cout << "[LightingSystem] Shadow maps initialized - Resolution: " << shadowMapResolution << std::endl;
    return true;
}

void LightingSystem::Update()
{
    PROFILE_FUNCTION();
    CollectLightData();
}

void LightingSystem::Shutdown()
{
    directionalShadowMap.Shutdown();
    shadowDepthShader = nullptr;
    std::cout << "[LightingSystem] Shutdown" << std::endl;
}

void LightingSystem::RenderShadowMaps()
{
    if (!shadowsEnabled || !directionalLightData.hasDirectionalLight)
    {
        return;
    }

    PROFILE_FUNCTION();
    RenderDirectionalShadowMap();
}

void LightingSystem::RenderDirectionalShadowMap()
{
    if (!shadowDepthShader || !directionalShadowMap.IsInitialized())
    {
        return;
    }

    // Debug - check if this runs every frame
    static int frameCount = 0;
    frameCount++;
    if (frameCount <= 5) {
        std::cout << "[Shadow] RenderDirectionalShadowMap called - frame " << frameCount << std::endl;
    }


    // Get the current camera to center the shadow map around
    Camera* camera = GraphicsManager::GetInstance().GetCurrentCamera();
    glm::vec3 sceneCenter = camera ? camera->Position : glm::vec3(0.0f);

    // Calculate light space matrix
    directionalShadowMap.CalculateLightSpaceMatrix(
        directionalLightData.direction,
        sceneCenter,
        shadowDistance
    );

    // Store current framebuffer AND viewport to restore later
    GLint previousFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Bind shadow map for writing
    directionalShadowMap.BindForWriting();

    // Use the depth shader
    shadowDepthShader->Activate();
    shadowDepthShader->setMat4("lightSpaceMatrix", directionalShadowMap.GetLightSpaceMatrix());

    // Render scene geometry (only depth)
    if (shadowRenderCallback)
    {
        shadowRenderCallback(*shadowDepthShader);
    }

    // Restore previous framebuffer and viewport
    glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

const glm::mat4& LightingSystem::GetDirectionalLightSpaceMatrix() const
{
    return directionalShadowMap.GetLightSpaceMatrix();
}

void LightingSystem::ApplyLighting(Shader& shader)
{
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
    // Debug - print light space matrix once
    static bool once = false;
    if (!once) {
        glm::mat4 lsm = directionalShadowMap.GetLightSpaceMatrix();
        std::cout << "[Shadow] LightSpaceMatrix[0]: " << lsm[0][0] << ", " << lsm[1][0] << ", " << lsm[2][0] << ", " << lsm[3][0] << std::endl;
        std::cout << "[Shadow] LightSpaceMatrix[3]: " << lsm[0][3] << ", " << lsm[1][3] << ", " << lsm[2][3] << ", " << lsm[3][3] << std::endl;
        std::cout << "[Shadow] Texture ID: " << directionalShadowMap.GetDepthTexture() << std::endl;
        once = true;
    }

    if (!shadowsEnabled || !directionalLightData.hasDirectionalLight)
    {
        shader.setBool("shadowsEnabled", false);
        return;
    }

    // Enable shadows
    shader.setBool("shadowsEnabled", true);

    // Set light space matrix
    shader.setMat4("lightSpaceMatrix", directionalShadowMap.GetLightSpaceMatrix());

    // Bind shadow map to texture unit 8 (leaving 0-7 for material textures)
    directionalShadowMap.BindForReading(8);
    shader.setInt("shadowMap", 8);

    // Set shadow parameters
    shader.setFloat("shadowBias", directionalShadowMap.bias);
    shader.setFloat("shadowNormalBias", directionalShadowMap.normalBias);
    shader.setFloat("shadowSoftness", directionalShadowMap.softness);
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

    for (const auto& entity : entities)
    {
        // Skip inactive entities
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue;
            }
        }

        // Collect directional light (first one only)
        if (ecsManager.HasComponent<DirectionalLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<DirectionalLightComponent>(entity);
            if (light.enabled && !directionalLightData.hasDirectionalLight)
            {
                directionalLightData.hasDirectionalLight = true;

                glm::vec3 direction(0.2f, -1.0f, -0.3f);
                if (ecsManager.HasComponent<Transform>(entity))
                {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);
                    glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                    glm::mat3 rotationMatrix = glm::mat3(worldMat);
                    direction = glm::normalize(rotationMatrix * direction);
                }

                directionalLightData.direction = direction;
                directionalLightData.ambient = light.ambient.ConvertToGLM();
                directionalLightData.diffuse = light.diffuse.ConvertToGLM();
                directionalLightData.specular = light.specular.ConvertToGLM();
                directionalLightData.intensity = light.intensity;
                // directionalLightData.castShadows = light.castShadows;  // Add this to component if needed
            }
        }

        // Collect point lights
        if (ecsManager.HasComponent<PointLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<PointLightComponent>(entity);

            if (light.enabled)
            {
                if (pointLightData.positions.size() < MAX_POINT_LIGHTS)
                {
                    glm::vec3 position(0.0f);
                    if (ecsManager.HasComponent<Transform>(entity))
                    {
                        auto& transform = ecsManager.GetComponent<Transform>(entity);
                        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                        position = glm::vec3(worldMat[3]);
                    }

                    pointLightData.positions.push_back(position);
                    pointLightData.ambient.push_back(light.ambient.ConvertToGLM());
                    pointLightData.diffuse.push_back(light.diffuse.ConvertToGLM());
                    pointLightData.specular.push_back(light.specular.ConvertToGLM());
                    pointLightData.constant.push_back(light.constant);
                    pointLightData.linear.push_back(light.linear);
                    pointLightData.quadratic.push_back(light.quadratic);
                    pointLightData.intensity.push_back(light.intensity);
                }
                else
                {
                    static bool pointLightWarningShown = false;
                    if (!pointLightWarningShown) {
                        std::cout << "[LightingSystem] Warning: Maximum point lights (" << MAX_POINT_LIGHTS
                            << ") reached. Additional point lights will be ignored." << std::endl;
                        pointLightWarningShown = true;
                    }
                }
            }
        }

        // Collect spot lights
        if (ecsManager.HasComponent<SpotLightComponent>(entity))
        {
            auto& light = ecsManager.GetComponent<SpotLightComponent>(entity);

            if (light.enabled)
            {
                if (spotLightData.positions.size() < MAX_SPOT_LIGHTS)
                {
                    glm::vec3 position(0.0f);
                    glm::vec3 direction(0.0f, 0.0f, -1.0f);

                    if (ecsManager.HasComponent<Transform>(entity))
                    {
                        auto& transform = ecsManager.GetComponent<Transform>(entity);
                        glm::mat4 worldMat = transform.worldMatrix.ConvertToGLM();
                        position = glm::vec3(worldMat[3]);
                        glm::mat3 rotationMatrix = glm::mat3(worldMat);
                        direction = glm::normalize(rotationMatrix * direction);
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
                        std::cout << "[LightingSystem] Warning: Maximum spot lights (" << MAX_SPOT_LIGHTS
                            << ") reached. Additional spot lights will be ignored." << std::endl;
                        spotLightWarningShown = true;
                    }
                }
            }
        }
    }
}