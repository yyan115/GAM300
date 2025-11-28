/* Start Header ************************************************************************/
/*!
\file       ParticleSystem.cpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu
\date       Oct 2, 2025
\brief      Implementation of the particle system for GPU-instanced particle rendering.
            Manages particle emission, physics simulation, lifetime tracking, and
            OpenGL buffer management for efficient rendering of large particle effects.
            Supports configurable emission rates, velocities, colors, sizes, and gravity.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "Graphics/Particle/ParticleSystem.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Graphics/EBO.h"
#include "TimeManager.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#include "Engine.h"
#include "Asset Manager/AssetManager.hpp"
#include "Performance/PerformanceProfiler.hpp"
#include "ECS/ActiveComponent.hpp"

/******************************************************************************/
/*!
\fn         bool ParticleSystem::Initialise()
\brief      Initializes the particle system for all entities with particle components

\details    Sets up OpenGL buffers (VAO, VBO, EBO) for instanced rendering of particles.
            Creates quad geometry and configures vertex attributes for both per-vertex
            (position, UV) and per-instance (position, color, size, rotation) data.
            Reserves memory for particle pools based on maxParticles setting.

\return     bool - Returns true if initialization is successful
*/
/******************************************************************************/
bool ParticleSystem::Initialise() 
{
    ENGINE_LOG_INFO("Particle System Initializing...");
//#ifndef ANDROID
    return InitialiseParticles(); // Same as for SpriteSystem, Android must delay particle initialisation.
//#else
//    return true;
//#endif
}

bool ParticleSystem::InitialiseParticles()
{
    if (particleSystemInitialised) return true;

    ENGINE_LOG_INFO("[ParticleSystem] InitialiseParticles");
#ifdef ANDROID
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "Thread ID: %ld", gettid());
#endif

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities)
    {
        auto& particleComp = ecsManager.GetComponent<ParticleComponent>(entity);

        // Get the texture and shader first.
        std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(particleComp.textureGUID);
        ENGINE_LOG_INFO("[ParticleSystem] Texture Path: " + texturePath);
        particleComp.texturePath = texturePath;
		if (!particleComp.texturePath.empty())
            particleComp.particleTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(particleComp.textureGUID, texturePath);
        std::string shaderPath = ResourceManager::GetPlatformShaderPath("particle");
        ENGINE_LOG_INFO("[ParticleSystem] Shader Path: " + shaderPath);
        particleComp.particleShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

        // Setup VAO
        particleComp.particleVAO = new VAO();
        particleComp.particleVAO->Bind();

        // Quad vertices: position (vec2) + UV (vec2) = 4 floats per vertex
        float quadVertices[] = {
            // positions     // uvs
            -0.5f, -0.5f,    0.0f, 0.0f,  // Bottom-left
             0.5f, -0.5f,    1.0f, 0.0f,  // Bottom-right
             0.5f,  0.5f,    1.0f, 1.0f,  // Top-right
            -0.5f,  0.5f,    0.0f, 1.0f   // Top-left
        };

        // Quad indices (2 triangles)
        std::vector<GLuint> quadIndices = {
            0, 1, 2,
            2, 3, 0
        };

        // Create quad VBO using the dynamic constructor then update with data
        particleComp.quadVBO = new VBO(sizeof(quadVertices), GL_STATIC_DRAW);
        particleComp.quadVBO->UpdateData(quadVertices, sizeof(quadVertices));

        // Create and bind EBO while VAO is active
        particleComp.quadEBO = new EBO(quadIndices);
        particleComp.quadEBO->Bind();

        // Setup vertex attributes for the quad
        // Position attribute (location 0) - vec2
        particleComp.particleVAO->LinkAttrib(*particleComp.quadVBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);
        // UV attribute (location 1) - vec2
        particleComp.particleVAO->LinkAttrib(*particleComp.quadVBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        // Create instance VBO (per-particle data, updated every frame)
        particleComp.instanceVBO = new VBO(particleComp.maxParticles * sizeof(ParticleInstanceData), GL_DYNAMIC_DRAW);

        // Setup instance attributes (divisor = 1 means advance per instance)
        // Position (location 2)
        particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 2, 3, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, position), 1);

        // Color (location 3)
        particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 3, 4, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, color), 1);

        // Size (location 4)
        particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 4, 1, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, size), 1);

        // Rotation (location 5)
        particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 5, 1, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, rotation), 1);

        particleComp.quadVBO->Unbind();
        particleComp.particleVAO->Unbind();
        //particleComp.quadEBO->Unbind();

        // Reserve particle pool
        particleComp.particles.reserve(particleComp.maxParticles);

        ENGINE_PRINT("[ParticleSystem] Initialized particle emitter for entity with ", particleComp.maxParticles, " max particles\n");
    }

    particleSystemInitialised = true;
    return true;
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::InitializeParticleComponent(ParticleComponent& particleComp)
\brief      Initializes a single particle component's rendering resources

\details    Helper function that sets up VAO, VBO, and EBO for a particle component.
            Configures quad geometry with position and UV attributes, and sets up
            instance buffer for per-particle data. Used for runtime particle system
            creation.

\param      particleComp - Reference to the particle component to initialize
*/
/******************************************************************************/
void ParticleSystem::InitializeParticleComponent(ParticleComponent& particleComp) 
{
    particleComp.particleVAO = new VAO();
    particleComp.particleVAO->Bind();

    float quadVertices[] = {
        -0.5f, -0.5f,    0.0f, 0.0f,
         0.5f, -0.5f,    1.0f, 0.0f,
         0.5f,  0.5f,    1.0f, 1.0f,
        -0.5f,  0.5f,    0.0f, 1.0f
    };

    std::vector<GLuint> quadIndices = {
        0, 1, 2,
        2, 3, 0
    };

    particleComp.quadVBO = new VBO(sizeof(quadVertices), GL_STATIC_DRAW);
    particleComp.quadVBO->UpdateData(quadVertices, sizeof(quadVertices));

    particleComp.quadEBO = new EBO(quadIndices);
    particleComp.quadEBO->Bind();

    particleComp.particleVAO->LinkAttrib(*particleComp.quadVBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);
    particleComp.particleVAO->LinkAttrib(*particleComp.quadVBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    particleComp.instanceVBO = new VBO( particleComp.maxParticles * sizeof(ParticleInstanceData), GL_DYNAMIC_DRAW);

    particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 2, 3, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, position), 1);
    particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 3, 4, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, color), 1);
    particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 4, 1, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, size), 1);
    particleComp.particleVAO->LinkAttrib(*particleComp.instanceVBO, 5, 1, GL_FLOAT, sizeof(ParticleInstanceData), (void*)offsetof(ParticleInstanceData, rotation), 1);

    particleComp.quadVBO->Unbind();
    particleComp.particleVAO->Unbind();
    //particleComp.quadEBO->Unbind();

    particleComp.particles.reserve(particleComp.maxParticles);
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::Update()
\brief      Main update loop for all particle systems

\details    Iterates through all entities with particle components and performs:
            - Particle physics updates (velocity, position, lifetime)
            - Particle emission based on emission rate
            - Dead particle removal
            - Instance buffer updates with current particle data
            - Submission of render data to GraphicsManager
            Also initializes any uninitialized particle components added at runtime.

\note       Only processes visible particle systems that are actively emitting
*/
/******************************************************************************/
void ParticleSystem::Update()
{
	PROFILE_FUNCTION();
//#ifdef ANDROID
//    // Ensure the EGL context is current
//    if (!WindowManager::GetPlatform()->MakeContextCurrent()) {
//        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ParticleSystem] Failed to make EGL context current in Update()");
//        return;
//    }
//
//    EGLDisplay display = eglGetCurrentDisplay();
//    EGLContext context = eglGetCurrentContext();
//    EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
//
//    if (display == EGL_NO_DISPLAY || context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE) {
//        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ParticleSystem] EGL CONTEXT NOT CURRENT - skipping draw!");
//        return;
//    }
//
//    // Additional check: verify the surface is still valid
//    EGLint surfaceWidth, surfaceHeight;
//    if (!eglQuerySurface(display, surface, EGL_WIDTH, &surfaceWidth) ||
//        !eglQuerySurface(display, surface, EGL_HEIGHT, &surfaceHeight)) {
//        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[ParticleSystem] EGL surface is invalid - skipping draw!");
//        return;
//    }
//    InitialiseParticles(); // For some reason Android's OpenGL context is not initialized yet, so have to put in Update.
//#endif

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();
    float dt = static_cast<float>(TimeManager::GetDeltaTime());

    for (const auto& entity : entities)
    {
        // Skip inactive entities
        if (ecsManager.HasComponent<ActiveComponent>(entity)) {
            auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
            if (!activeComp.isActive) {
                continue; // Don't render inactive entities
            }
        }

        auto& particleComp = ecsManager.GetComponent<ParticleComponent>(entity);

        // Initialize if not already done
        if (!particleComp.particleVAO) // In case new particle system is added on the fly
        {
            InitializeParticleComponent(particleComp);
        }

        if (!particleComp.isVisible) continue;

        // Only update particle physics if:
        // 1. Game is running (NOT paused), OR
        // 2. Playing in editor AND not paused in editor
        bool shouldUpdateParticles = Engine::ShouldRunGameLogic() ||
                                    (particleComp.isPlayingInEditor && !particleComp.isPausedInEditor);

        if (!shouldUpdateParticles) {
            // Still submit to renderer (to show existing particles), but don't update physics or emit new particles
            auto renderItem = std::make_unique<ParticleComponent>(particleComp);
            gfxManager.Submit(std::move(renderItem));
            continue;
        }

        // Update particle physics
        UpdateParticles(particleComp, dt);

        // Get emitter position from Transform component
        if (ecsManager.HasComponent<Transform>(entity))
        {
            auto& transform = ecsManager.GetComponent<Transform>(entity);
            particleComp.emitterPosition = Vector3D(
                transform.worldMatrix.m.m03,
                transform.worldMatrix.m.m13,
                transform.worldMatrix.m.m23
            );
        }

        // Emit new particles
        if (particleComp.isEmitting) 
        {
            EmitParticles(particleComp, dt);
        }

        // Remove dead particles
        RemoveDeadParticles(particleComp);

        // Update instance buffer with current particle data
        UpdateInstanceBuffer(particleComp);

        // Submit to renderer
        auto renderItem = std::make_unique<ParticleComponent>(particleComp);
        gfxManager.Submit(std::move(renderItem));
    }
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::Shutdown()
\brief      Cleans up all particle system resources

\details    Deallocates all OpenGL buffers (VAO, VBO, EBO) for each particle
            component and sets pointers to nullptr to prevent dangling references.
            Should be called before destroying the particle system.
*/
/******************************************************************************/

void ParticleSystem::Shutdown()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities)
    {
        auto& particleComp = ecsManager.GetComponent<ParticleComponent>(entity);

        if (particleComp.particleVAO)
        {
            delete particleComp.particleVAO;
            particleComp.particleVAO = nullptr;
        }
        if (particleComp.quadVBO) 
        {
            delete particleComp.quadVBO;
            particleComp.quadVBO = nullptr;
        }
        if (particleComp.quadEBO) 
        {
            delete particleComp.quadEBO;
            particleComp.quadEBO = nullptr;
        }
        if (particleComp.instanceVBO) 
        {
            delete particleComp.instanceVBO;
            particleComp.instanceVBO = nullptr;
        }
    }

    particleSystemInitialised = false;
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::UpdateParticles(ParticleComponent& comp, float dt)
\brief      Updates physics and visual properties of all particles in a component

\details    Applies gravity to particle velocities, updates positions based on
            velocity, decrements particle lifetime, and interpolates size and color
            between start and end values based on normalized lifetime (0 = birth, 1 = death).

\param      comp - Reference to the particle component to update
\param      dt - Delta time in seconds since last frame
*/
/******************************************************************************/

void ParticleSystem::UpdateParticles(ParticleComponent& comp, float dt)
{
    for (auto& particle : comp.particles) 
    {
        // Update physics
        particle.velocity += comp.gravity.ConvertToGLM() * dt;
        particle.position += particle.velocity * dt;

        // Update life
        particle.life -= dt / comp.particleLifetime;

        // Interpolate properties based on life
        float t = 1.0f - particle.life;
        particle.size = glm::mix(comp.startSize, comp.endSize, t);
        glm::vec4 startColor{ comp.startColor.x, comp.startColor.y, comp.startColor.z, comp.startColorAlpha };
        glm::vec4 endColor{ comp.endColor.x, comp.endColor.y, comp.endColor.z, comp.endColorAlpha };
        particle.color = glm::mix(startColor, endColor, t);
    }
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::EmitParticles(ParticleComponent& comp, float dt)
\brief      Spawns new particles based on emission rate and settings

\details    Accumulates emission time and spawns particles at regular intervals
            determined by emission rate. Each new particle is initialized with
            emitter position, start properties (size, color), random rotation,
            and initial velocity with randomness applied. Respects maxParticles limit.

\param      comp - Reference to the particle component to emit from
\param      dt - Delta time in seconds since last frame
*/
/******************************************************************************/
void ParticleSystem::EmitParticles(ParticleComponent& comp, float dt)
{
    comp.timeSinceEmission += dt;
    float emissionInterval = 1.0f / comp.emissionRate;

    while (comp.timeSinceEmission >= emissionInterval) 
    {
        comp.timeSinceEmission -= emissionInterval;

        if (comp.particles.size() >= comp.maxParticles) break;

        Particle p;
        p.position = comp.emitterPosition.ConvertToGLM();
        p.life = 1.0f;
        p.size = comp.startSize;
        p.color = glm::vec4{ comp.startColor.x, comp.startColor.y, comp.startColor.z, comp.startColorAlpha };
        p.rotation = dist(rng) * 360.0f;

        // Add velocity randomness
        glm::vec3 randomVel(
            dist(rng) * comp.velocityRandomness,
            dist(rng) * comp.velocityRandomness,
            dist(rng) * comp.velocityRandomness
        );
        p.velocity = comp.initialVelocity.ConvertToGLM() + randomVel;

        comp.particles.push_back(p);
    }
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::RemoveDeadParticles(ParticleComponent& comp)
\brief      Removes particles that have exceeded their lifetime

\details    Uses erase-remove idiom to efficiently remove all particles with
            life <= 0.0f from the particle vector, maintaining contiguous memory.

\param      comp - Reference to the particle component to clean up
*/
/******************************************************************************/
void ParticleSystem::RemoveDeadParticles(ParticleComponent& comp)
{
    comp.particles.erase(
        std::remove_if(comp.particles.begin(), comp.particles.end(),
            [](const Particle& p) { return p.life <= 0.0f; }),
        comp.particles.end()
    );
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::UpdateInstanceBuffer(ParticleComponent& comp)
\brief      Uploads current particle data to GPU instance buffer

\details    Builds an array of ParticleInstanceData from active particles containing
            position, color, size, and rotation for each particle, then updates the
            instance VBO for use in instanced rendering. Early exits if no particles exist.

\param      comp - Reference to the particle component whose buffer needs updating
*/
/******************************************************************************/
void ParticleSystem::UpdateInstanceBuffer(ParticleComponent& comp)
{
    if (comp.particles.empty()) return;

    // Prepare instance data array
    std::vector<ParticleInstanceData> instanceData;
    instanceData.reserve(comp.particles.size());

    for (const auto& particle : comp.particles)
    {
        ParticleInstanceData data;
        data.position = particle.position;
        data.color = particle.color;
        data.size = particle.size;
        data.rotation = particle.rotation;
        instanceData.push_back(data);
    }

    // Update the instance VBO
    comp.instanceVBO->Bind();
    comp.instanceVBO->UpdateData(instanceData.data(), instanceData.size() * sizeof(ParticleInstanceData));
    comp.instanceVBO->Unbind();
}
