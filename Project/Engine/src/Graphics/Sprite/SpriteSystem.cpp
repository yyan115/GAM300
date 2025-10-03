#include "pch.h"
#include "Graphics/Sprite/SpriteSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Transform/TransformComponent.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/EBO.h"

bool SpriteSystem::Initialise()
{
    InitializeSpriteQuad();
	std::cout << "[SpriteSystem] Initialized" << std::endl;
	return true;
}

void SpriteSystem::Update()
{
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "SpriteSystem::Update() called");
#endif
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();

#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "SpriteSystem entities count: %zu", entities.size());
#endif

    // Submit all visible sprites to the graphics manager
    for (const auto& entity : entities)
    {
#ifdef ANDROID
        //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Processing sprite entity: %u", entity);
#endif
        auto& spriteComponent = ecsManager.GetComponent<SpriteRenderComponent>(entity);

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

            // For 3D sprites, update position from transform component
            if (spriteComponent.is3D && ecsManager.HasComponent<Transform>(entity)) 
            {
                auto& transform = ecsManager.GetComponent<Transform>(entity);
                // Convert world matrix to position (you might want to extract scale/rotation too)
                spriteRenderItem->position = glm::vec3(transform.worldMatrix.m.m03,
                    transform.worldMatrix.m.m13,
                    transform.worldMatrix.m.m23);
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

    // Create EBO using your existing class
    spriteEBO = std::make_unique<EBO>(indices);

    // Use your VBO class for vertex data
    spriteVBO = std::make_unique<VBO>(sizeof(vertices), GL_STATIC_DRAW);
    spriteVBO->UpdateData(vertices, sizeof(vertices));

    spriteVAO = std::make_unique<VAO>();
    spriteVAO->Bind();

    // Bind EBO to VAO before setting up vertex attributes
    spriteEBO->Bind();

    // Position attribute
    spriteVAO->LinkAttrib(*spriteVBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);

    // Texture coordinate attribute  
    spriteVAO->LinkAttrib(*spriteVBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    spriteVAO->Unbind();
    spriteVBO->Unbind();
    spriteEBO->Unbind();

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
