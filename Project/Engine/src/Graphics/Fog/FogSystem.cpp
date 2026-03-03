#include "pch.h"
#include "Graphics/Fog/FogSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/EBO.h"
#include "TimeManager.hpp"
#include "Engine.h"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "ECS/ActiveComponent.hpp"

bool FogSystem::Initialise(bool forceInit)
{
    if (fogSystemInitialised && !forceInit) return true;

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities)
    {
        auto& fogComp = ecsManager.GetComponent<FogVolumeComponent>(entity);
        // Load fog shader
        std::string shaderPath = ResourceManager::GetPlatformShaderPath("fog");
        ENGINE_LOG_INFO("[FogSystem] Shader Path: " + shaderPath);
        fogComp.fogShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

        // Load noise texture if GUID is set
        if (fogComp.noiseTextureGUID.high != 0 || fogComp.noiseTextureGUID.low != 0)
        {
            std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(fogComp.noiseTextureGUID);
            fogComp.noiseTexturePath = texturePath;
            if (!texturePath.empty())
            {
                fogComp.noiseTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(
                    fogComp.noiseTextureGUID, texturePath);
            }
        }
        InitializeFogComponent(fogComp);

        ENGINE_PRINT("[FogSystem] Initialized fog volume for entity\n");

    }

    fogSystemInitialised = true;
    return true;
}

void FogSystem::Update()
{
    PROFILE_FUNCTION();

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    for (const auto& entity : entities)
    {
        // Skip entities that are inactive in hierarchy
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        auto& fogComp = ecsManager.GetComponent<FogVolumeComponent>(entity);

        // Initialize if not already done (added at runtime)
        if (!fogComp.fogVAO)
        {
            // Load shader if missing
            if (!fogComp.fogShader)
            {
                std::string shaderPath = ResourceManager::GetPlatformShaderPath("fog");
                fogComp.fogShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
            }

            // Load noise texture if GUID set but texture not loaded
            if (!fogComp.noiseTexture && (fogComp.noiseTextureGUID.high != 0 || fogComp.noiseTextureGUID.low != 0))
            {
                std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(fogComp.noiseTextureGUID);
                fogComp.noiseTexturePath = texPath;
                if (!texPath.empty())
                {
                    fogComp.noiseTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(
                        fogComp.noiseTextureGUID, texPath);
                }
            }

            InitializeFogComponent(fogComp);
        }

        if (!fogComp.isVisible) continue;

        // Get world transform from entity's Transform component
        if (ecsManager.HasComponent<Transform>(entity))
        {
            auto& transform = ecsManager.GetComponent<Transform>(entity);
            fogComp.worldTransform = transform.worldMatrix;
        }

        // Submit to renderer (fog always renders, no physics tick needed)
        auto renderItem = std::make_unique<FogVolumeComponent>(fogComp);
        gfxManager.Submit(std::move(renderItem));
    }
}

void FogSystem::Shutdown()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities)
    {
        auto& fogComp = ecsManager.GetComponent<FogVolumeComponent>(entity);

        if (fogComp.fogVAO)
        {
            delete fogComp.fogVAO;
            fogComp.fogVAO = nullptr;
        }
        if (fogComp.fogVBO)
        {
            delete fogComp.fogVBO;
            fogComp.fogVBO = nullptr;
        }
        if (fogComp.fogEBO)
        {
            delete fogComp.fogEBO;
            fogComp.fogEBO = nullptr;
        }
    }

    fogSystemInitialised = false;
    ENGINE_PRINT("[FogSystem] Shutdown\n");
}

void FogSystem::InitializeFogComponent(FogVolumeComponent& fogComp)
{
    fogComp.fogVAO = new VAO();
    fogComp.fogVAO->Bind();

    // Unit cube vertices [-0.5, 0.5] — 24 verts (4 per face, for proper winding)
    float cubeVertices[] = {
        // Front face
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        // Back face
        -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         // Top face
         -0.5f,  0.5f, -0.5f,
         -0.5f,  0.5f,  0.5f,
          0.5f,  0.5f,  0.5f,
          0.5f,  0.5f, -0.5f,
          // Bottom face
          -0.5f, -0.5f, -0.5f,
           0.5f, -0.5f, -0.5f,
           0.5f, -0.5f,  0.5f,
          -0.5f, -0.5f,  0.5f,
          // Right face
           0.5f, -0.5f, -0.5f,
           0.5f,  0.5f, -0.5f,
           0.5f,  0.5f,  0.5f,
           0.5f, -0.5f,  0.5f,
           // Left face
           -0.5f, -0.5f, -0.5f,
           -0.5f, -0.5f,  0.5f,
           -0.5f,  0.5f,  0.5f,
           -0.5f,  0.5f, -0.5f,
    };

    std::vector<GLuint> cubeIndices = {
        0,  1,  2,  2,  3,  0,   // Front
        4,  5,  6,  6,  7,  4,   // Back
        8,  9,  10, 10, 11, 8,   // Top
        12, 13, 14, 14, 15, 12,  // Bottom
        16, 17, 18, 18, 19, 16,  // Right
        20, 21, 22, 22, 23, 20   // Left
    };

    // Create VBO with static draw (cube doesn't change)
    fogComp.fogVBO = new VBO(sizeof(cubeVertices), GL_STATIC_DRAW);
    fogComp.fogVBO->UpdateData(cubeVertices, sizeof(cubeVertices));

    // Create EBO
    fogComp.fogEBO = new EBO(cubeIndices);
    fogComp.fogEBO->Bind();

    // Position attribute only (location 0) — vec3
    fogComp.fogVAO->LinkAttrib(*fogComp.fogVBO, 0, 3, GL_FLOAT, 3 * sizeof(float), (void*)0);

    fogComp.fogVBO->Unbind();
    fogComp.fogVAO->Unbind();
}
