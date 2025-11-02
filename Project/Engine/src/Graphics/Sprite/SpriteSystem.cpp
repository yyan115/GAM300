#include "pch.h"
#include "Graphics/Sprite/SpriteSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/EBO.h"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Performance/PerformanceProfiler.hpp"

bool SpriteSystem::Initialise()
{
#ifndef ANDROID
    InitializeSpriteQuad();
#endif

    auto& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    for (const auto& entity : entities) {
        auto& spriteComp = ecsManager.GetComponent<SpriteRenderComponent>(entity);
        std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteComp.textureGUID);
        spriteComp.texturePath = texturePath;
        spriteComp.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(spriteComp.textureGUID, texturePath);
#ifndef ANDROID
        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteComp.shaderGUID);
        spriteComp.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(spriteComp.shaderGUID, shaderPath);
#else
        std::string shaderPath = ResourceManager::GetPlatformShaderPath("sprite");
        spriteComp.shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

#endif
    }

    ENGINE_LOG_INFO("SpriteSystem Initialized");
	return true;
}

void SpriteSystem::Update()
{
	PROFILE_FUNCTION();
#ifdef ANDROID
    InitializeSpriteQuad(); // For some reason Android's OpenGL context is not initialized yet, so have to put in Update.
#endif

#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "SpriteSystem::Update() called");
#endif
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

    // Get current view mode and check if rendering for editor
    bool isRenderingForEditor = gfxManager.IsRenderingForEditor();
    bool is3DMode = gfxManager.Is3DMode();
    bool is2DMode = gfxManager.Is2DMode();

#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "SpriteSystem entities count: %zu", entities.size());
#endif

    // Submit all visible sprites to the graphics manager
    for (const auto& entity : entities)
    {
        // Skip inactive entities (Unity-like behavior)
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue; // Don't render inactive entities
            }
        }

#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Processing sprite entity: %u", entity);
#endif
        auto& spriteComponent = ecsManager.GetComponent<SpriteRenderComponent>(entity);

        // Filter sprites based on view mode ONLY when rendering for editor
        // Game window should always show all sprites
        if (isRenderingForEditor) {
            // In 3D mode: only show 3D sprites
            // In 2D mode: only show 2D sprites
            if (is3DMode && !spriteComponent.is3D) {
                continue; // Skip 2D sprites in 3D mode (editor only)
            }
            if (is2DMode && spriteComponent.is3D) {
                continue; // Skip 3D sprites in 2D mode (editor only)
            }
        }

        spriteComponent.spriteVAO = spriteVAO.get();
        spriteComponent.spriteEBO = spriteEBO.get();
#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Entity %u: isVisible=%d, texture=%p, shader=%p",
        //                 entity, spriteComponent.isVisible, spriteComponent.texture.get(), spriteComponent.shader.get());
#endif
        if (spriteComponent.isVisible && spriteComponent.texture && spriteComponent.shader)
        {
#ifdef ANDROID
            // __android_log_print(ANDROID_LOG_INFO, "GAM300", "Submitting sprite for entity: %u", entity);
#endif
            auto spriteRenderItem = std::make_unique<SpriteRenderComponent>(spriteComponent);

            // Copy the VAO pointer to the render item
            spriteRenderItem->spriteVAO = spriteVAO.get();

            // For both 2D and 3D sprites, update position/scale/rotation from Transform component if it exists
            if (ecsManager.HasComponent<Transform>(entity))
            {
                auto& transform = ecsManager.GetComponent<Transform>(entity);

                // Extract position from world matrix
                glm::vec3 transformPos = glm::vec3(transform.worldMatrix.m.m03,
                    transform.worldMatrix.m.m13,
                    transform.worldMatrix.m.m23);

                // For 2D sprites: If Transform position is at origin (0,0,0) but sprite has a different position,
                // it means the sprite was created with position in sprite component, not Transform
                // In this case, sync Transform to sprite position and scale
                if (!spriteComponent.is3D && !spriteComponent.hasMigratedToTransform &&
                    transformPos.x == 0.0f && transformPos.y == 0.0f && transformPos.z == 0.0f &&
                    !(spriteComponent.position.x == 0.0f && spriteComponent.position.y == 0.0f && spriteComponent.position.z == 0.0f))
                {
                    // Sync Transform to sprite position and scale (one-time migration)
                    ecsManager.transformSystem->SetWorldPosition(entity,
                        Vector3D(spriteComponent.position.x, spriteComponent.position.y, spriteComponent.position.z));
                    ecsManager.transformSystem->SetWorldScale(entity,
                        Vector3D(spriteComponent.scale.x, spriteComponent.scale.y, spriteComponent.scale.z));
                    transformPos = spriteComponent.position.ConvertToGLM();
                    spriteComponent.hasMigratedToTransform = true;

                    // Log the migration (only once)
                    std::cout << "[SpriteSystem] Migrated 2D sprite " << entity << " from sprite properties to Transform: "
                              << "pos(" << spriteComponent.position.x << "," << spriteComponent.position.y << ") "
                              << "scale(" << spriteComponent.scale.x << "," << spriteComponent.scale.y << ")" << std::endl;
                }

                spriteRenderItem->position = Vector3D::ConvertGLMToVector3D(transformPos);

                // Extract scale from world matrix (length of basis vectors)
                float scaleX = sqrt(transform.worldMatrix.m.m00 * transform.worldMatrix.m.m00 +
                                   transform.worldMatrix.m.m10 * transform.worldMatrix.m.m10 +
                                   transform.worldMatrix.m.m20 * transform.worldMatrix.m.m20);
                float scaleY = sqrt(transform.worldMatrix.m.m01 * transform.worldMatrix.m.m01 +
                                   transform.worldMatrix.m.m11 * transform.worldMatrix.m.m11 +
                                   transform.worldMatrix.m.m21 * transform.worldMatrix.m.m21);
                float scaleZ = sqrt(transform.worldMatrix.m.m02 * transform.worldMatrix.m.m02 +
                                   transform.worldMatrix.m.m12 * transform.worldMatrix.m.m12 +
                                   transform.worldMatrix.m.m22 * transform.worldMatrix.m.m22);

                spriteRenderItem->scale = Vector3D::ConvertGLMToVector3D(glm::vec3(scaleX, scaleY, scaleZ));

                // Extract Z-axis rotation from the world matrix for 2D sprites
                // Calculate rotation from the 2D rotation matrix (using X and Y basis vectors)
                float rotationRadians = atan2(transform.worldMatrix.m.m10, transform.worldMatrix.m.m00);
                spriteRenderItem->rotation = glm::degrees(rotationRadians);

            } else {
                // No Transform component - use sprite's own properties
            }

                gfxManager.Submit(std::move(spriteRenderItem));
        }
#ifdef ANDROID
        else {
            //__android_log_print(ANDROID_LOG_WARN, "GAM300", "Entity %u: sprite not visible or missing components - isVisible=%d, texture=%p, shader=%p",
                       //       entity, spriteComponent.isVisible, spriteComponent.texture.get(), spriteComponent.shader.get());
        }
#endif
    }
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "SpriteSystem::Update() completed");
#endif
}

void SpriteSystem::Shutdown()
{
    CleanupSpriteQuad();
    std::cout << "[SpriteSystem] Shutdown" << std::endl;
}

void SpriteSystem::InitializeSpriteQuad()
{
    if (spriteQuadInitialized) return;
    ENGINE_LOG_INFO("[SpriteSystem] InitializeSpriteQuad");
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Thread ID: %ld", gettid());
#endif

    // Define a unit quad (0,0 to 1,1) with texture coordinates - only 4 vertices
    float vertices[] = {
        // Position     // TexCoords
        0.0f, 1.0f,     0.0f, 1.0f,  // Top-left     (0)
        1.0f, 1.0f,     1.0f, 1.0f,  // Top-right    (1)
        1.0f, 0.0f,     1.0f, 0.0f,  // Bottom-right (2)
        0.0f, 0.0f,     0.0f, 0.0f   // Bottom-left  (3)
    };

    // Index buffer - defines two triangles to form a quad
    std::vector<GLuint> indices = {
        0, 1, 2,  // First triangle  (top-left, top-right, bottom-right)
        2, 3, 0   // Second triangle (bottom-right, bottom-left, top-left)
    };


    spriteVAO = std::make_unique<VAO>();
    spriteVAO->Bind();

    // Use your VBO class for vertex data
    spriteVBO = std::make_unique<VBO>(sizeof(vertices), GL_STATIC_DRAW);
    spriteVBO->UpdateData(vertices, sizeof(vertices));

    // Create EBO using your existing class
    spriteEBO = std::make_unique<EBO>(indices);
    // Bind EBO to VAO before setting up vertex attributes
    spriteEBO->Bind();

    // Position attribute
    spriteVAO->LinkAttrib(*spriteVBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);

    // Texture coordinate attribute  
    spriteVAO->LinkAttrib(*spriteVBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    spriteVBO->Unbind();
    spriteVAO->Unbind();
    //spriteEBO->Unbind();

    spriteVAO->Bind();
    GLint eboBinding = 0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eboBinding);
    assert(eboBinding != 0 && "VAO has no EBO bound after setup");
    spriteVAO->Unbind();


    spriteQuadInitialized = true;

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[SpriteSystem] Sprite quad initialized using VBO/VAO/EBO classes with indices");
#endif
    std::cout << "[SpriteSystem] Sprite quad initialized with indexed rendering" << std::endl;
}

void SpriteSystem::CleanupSpriteQuad()
{
    if (spriteQuadInitialized) 
    {
        if (spriteVAO) 
        {
            spriteVAO->Delete();
            spriteVAO.reset();
        }
        if (spriteVBO) 
        {
            spriteVBO->Delete();
            spriteVBO.reset();
        }
        if (spriteEBO) 
        {
            spriteEBO->Delete();
            spriteEBO.reset();
        }
        spriteQuadInitialized = false;
        std::cout << "[SpriteSystem] Sprite quad cleaned up" << std::endl;
    }
}
