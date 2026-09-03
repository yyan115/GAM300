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
#include "ECS/ActiveComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Graphics/BloomComponent.hpp"
#include "Graphics/Particle/ParticleRenderItem.hpp"
namespace {
constexpr float kPi = 3.14159265358979323846f;

#ifdef ANDROID
std::uint16_t PackParticleUnorm(float value)
{
    return static_cast<std::uint16_t>(
        glm::clamp(value, 0.0f, 1.0f) * 65535.0f + 0.5f);
}

std::int16_t PackParticleSnorm(float value)
{
    return static_cast<std::int16_t>(std::lround(
        glm::clamp(value, -1.0f, 1.0f) * 32767.0f));
}
#endif

void LinkParticleInstanceAttributes(VAO& vao, VBO& instanceVBO)
{
    vao.LinkAttrib(instanceVBO, 2, 3, GL_FLOAT, sizeof(ParticleInstanceData),
        reinterpret_cast<void*>(offsetof(ParticleInstanceData, position)), 1);
#ifdef ANDROID
    vao.LinkAttribNormalized(instanceVBO, 3, 1, GL_UNSIGNED_SHORT, sizeof(ParticleInstanceData),
        reinterpret_cast<void*>(offsetof(ParticleInstanceData, lifeUnorm)), 1);
    vao.LinkAttribNormalized(instanceVBO, 4, 2, GL_SHORT, sizeof(ParticleInstanceData),
        reinterpret_cast<void*>(offsetof(ParticleInstanceData, rotationSinCos)), 1);
#else
    vao.LinkAttrib(instanceVBO, 3, 4, GL_FLOAT, sizeof(ParticleInstanceData),
        reinterpret_cast<void*>(offsetof(ParticleInstanceData, color)), 1);
    vao.LinkAttrib(instanceVBO, 4, 1, GL_FLOAT, sizeof(ParticleInstanceData),
        reinterpret_cast<void*>(offsetof(ParticleInstanceData, size)), 1);
    vao.LinkAttrib(instanceVBO, 5, 2, GL_FLOAT, sizeof(ParticleInstanceData),
        reinterpret_cast<void*>(offsetof(ParticleInstanceData, rotationSinCos)), 1);
#endif
}

bool BuildRenderItem(
    ParticleRenderItem& renderItem,
    const ParticleComponent& particleComp,
    Entity entity,
    ECSManager& ecsManager,
    GraphicsManager& graphicsManager,
    uint32_t excludedLayerMask)
{
    if (particleComp.particles.empty() || !particleComp.particleShader || !particleComp.particleVAO) {
        return false;
    }

#ifdef __ANDROID__
	if (particleComp.hasParticleBounds &&
		graphicsManager.IsFrustumCullingEnabled() &&
		!graphicsManager.GetFrustum().IsBoxVisible(AABB(
			particleComp.particleBoundsMin,
			particleComp.particleBoundsMax))) {
		return false;
	}
#endif

    renderItem.isVisible = particleComp.isVisible;
    renderItem.renderOrder = particleComp.renderOrder;
    renderItem.excludeFromPostProcess = particleComp.excludeFromPostProcess;
    renderItem.bloomColor = particleComp.bloomColor;
    renderItem.bloomIntensity = particleComp.bloomIntensity;
    renderItem.brightnessBoost = particleComp.brightnessBoost;
    renderItem.particleTexture = particleComp.particleTexture;
    renderItem.particleShader = particleComp.particleShader;
    renderItem.particleVAO = particleComp.particleVAO;
    renderItem.quadEBO = particleComp.quadEBO;
    renderItem.particleCount = particleComp.particles.size();
    renderItem.additiveBlending = particleComp.additiveBlending;
    renderItem.startColor = glm::vec4(
        particleComp.startColor.ConvertToGLM(), particleComp.startColorAlpha);
    renderItem.endColor = glm::vec4(
        particleComp.endColor.ConvertToGLM(), particleComp.endColorAlpha);
    renderItem.startSize = particleComp.startSize;
    renderItem.endSize = particleComp.endSize;

    if (auto bloomComponent = ecsManager.TryGetComponent<BloomComponent>(entity)) {
        const auto& bloom = bloomComponent->get();
        if (bloom.enabled) {
            renderItem.bloomColor = bloom.bloomColor;
            renderItem.bloomIntensity = bloom.bloomIntensity;
        }
    }

    if (excludedLayerMask != 0) {
        const int layerIndex = GetEffectiveLayerIndex(entity, ecsManager);
        if (layerIndex >= 0 && layerIndex < 32 && (excludedLayerMask & (1u << layerIndex))) {
            renderItem.excludeFromPostProcess = true;
        }
    }

    if (!renderItem.excludeFromPostProcess &&
        renderItem.bloomIntensity > 0.01f) {
        graphicsManager.NotifyBloomUsedThisFrame();
    }

    return true;
}
}

/******************************************************************************/
/*!
\fn         bool ParticleSystem::Initialise()
\brief      Initializes the particle system for all entities with particle components

\details    Sets up OpenGL buffers (VAO, VBO, EBO) for instanced rendering of particles.
            Creates quad geometry and configures vertex attributes for both per-vertex
            (position, UV) and per-instance (position, color, size, rotation
            sine/cosine) data.
            Reserves memory for particle pools based on maxParticles setting.

\return     bool - Returns true if initialization is successful
*/
/******************************************************************************/
bool ParticleSystem::Initialise(bool forceInit) 
{
    ENGINE_LOG_INFO("Particle System Initializing...");
//#ifndef ANDROID
    return InitialiseParticles(forceInit); // Same as for SpriteSystem, Android must delay particle initialisation.
//#else
//    return true;
//#endif
}

bool ParticleSystem::InitialiseParticles(bool forceInit)
{
    if (particleSystemInitialised && !forceInit) return true;

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

        // Cap maxParticles on Android to avoid oversized buffers on mobile GPU
#ifdef __ANDROID__
        if (particleComp.maxParticles > 300)
            particleComp.maxParticles = 300;
#endif

        // Create instance VBO (per-particle data, updated every frame)
        particleComp.instanceVBO = new VBO(particleComp.maxParticles * sizeof(ParticleInstanceData), GL_DYNAMIC_DRAW);
        particleComp.instanceVBO->Bind();  // Must bind before LinkAttrib

        // Divisor 1 advances these attributes once per particle instance.
        LinkParticleInstanceAttributes(*particleComp.particleVAO, *particleComp.instanceVBO);

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
    // Get the texture and shader first.
    std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(particleComp.textureGUID);
    ENGINE_LOG_INFO("[ParticleSystem] Texture Path: " + texturePath);
    particleComp.texturePath = texturePath;
    if (!particleComp.texturePath.empty())
        particleComp.particleTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(particleComp.textureGUID, texturePath);
    std::string shaderPath = ResourceManager::GetPlatformShaderPath("particle");
    ENGINE_LOG_INFO("[ParticleSystem] Shader Path: " + shaderPath);
    particleComp.particleShader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);

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

    // Cap maxParticles on Android to avoid oversized buffers on mobile GPU
#ifdef __ANDROID__
    if (particleComp.maxParticles > 300)
        particleComp.maxParticles = 300;
#endif

    particleComp.instanceVBO = new VBO( particleComp.maxParticles * sizeof(ParticleInstanceData), GL_DYNAMIC_DRAW);
    particleComp.instanceVBO->Bind();  // Must bind before LinkAttrib

    LinkParticleInstanceAttributes(*particleComp.particleVAO, *particleComp.instanceVBO);

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
    const float dt = static_cast<float>(TimeManager::GetDeltaTime());
    const bool shouldRunGameLogic = Engine::ShouldRunGameLogic();
    Camera* const currentCamera = gfxManager.GetCurrentCamera();
    const uint32_t excludedLayerMask = PostProcessingManager::GetInstance().GetExcludedLayerMask();
    renderSnapshots.reserve(entities.size());
    std::size_t renderSnapshotCount = 0;

    for (const auto& entity : entities)
    {
        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        auto& particleComp = ecsManager.GetComponent<ParticleComponent>(entity);

        // Initialize if not already done
        if (!particleComp.particleVAO) // In case new particle system is added on the fly
        {
            InitializeParticleComponent(particleComp);
        }

        if (!particleComp.isVisible) continue;

        Transform* transform = nullptr;
        if (auto transformComponent = ecsManager.TryGetComponent<Transform>(entity)) {
            transform = &transformComponent->get();
        }

#ifdef __ANDROID__
        // Distance culling: skip simulation + rendering for emitters far from camera
        if (transform && currentCamera)
        {
            const glm::vec3 diff =
                transform->worldPosition.ConvertToGLM() - currentCamera->Position;
            if (glm::dot(diff, diff) > 900.0f) // 30 units
                continue;
        }

#endif

        // Only update particle physics if:
        // 1. Game is running (NOT paused), OR
        // 2. Playing in editor AND not paused in editor
        const bool shouldUpdateParticles = shouldRunGameLogic ||
                                    (particleComp.isPlayingInEditor && !particleComp.isPausedInEditor);

        if (shouldUpdateParticles) {
            // Update particle physics
            UpdateParticles(particleComp, dt);

            // Calculate world emission position (transform + local offset)
            glm::vec3 emitterWorldPos = particleComp.emitterPosition.ConvertToGLM();
            if (transform) {
                emitterWorldPos += transform->worldPosition.ConvertToGLM();
            }

            // Emit new particles
            if (particleComp.isEmitting)
            {
                float effectiveEmissionRate = particleComp.emissionRate;
#ifdef __ANDROID__
                effectiveEmissionRate *= 0.5f;
#endif
                EmitParticles(particleComp, dt, emitterWorldPos, effectiveEmissionRate);
            }

            // UpdateParticles compacts expired particles in the same pass.
            UpdateInstanceBuffer(particleComp);
        }

        // The GPU instance buffer contains all per-particle data. Queue only a
        // lightweight draw snapshot, including while simulation is paused.
        if (renderSnapshotCount == renderSnapshots.size()) {
            renderSnapshots.emplace_back();
        }
        if (BuildRenderItem(
                renderSnapshots[renderSnapshotCount],
                particleComp,
                entity,
                ecsManager,
                gfxManager,
                excludedLayerMask)) {
            ++renderSnapshotCount;
        }
    }
    gfxManager.SubmitBatch(renderSnapshots, renderSnapshotCount);
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
    const glm::vec3 gravityStep = comp.gravity.ConvertToGLM() * dt;
    const float lifeStep = dt / comp.particleLifetime;
#ifndef ANDROID
    const float sizeDelta = comp.endSize - comp.startSize;
    const glm::vec4 startColor{
        comp.startColor.x, comp.startColor.y, comp.startColor.z, comp.startColorAlpha
    };
    const glm::vec4 endColor{
        comp.endColor.x, comp.endColor.y, comp.endColor.z, comp.endColorAlpha
    };
    const glm::vec4 colorDelta = endColor - startColor;
#endif

    std::size_t aliveCount = 0;
    for (std::size_t index = 0; index < comp.particles.size(); ++index)
    {
        Particle& particle = comp.particles[index];

        particle.life -= lifeStep;
        if (particle.life <= 0.0f) {
            continue;
        }

        // Update physics
        particle.velocity += gravityStep;
        particle.position += particle.velocity * dt;

        // Interpolate properties based on life
#ifndef ANDROID
        const float t = 1.0f - particle.life;
        particle.size = comp.startSize + sizeDelta * t;
        particle.color = startColor + colorDelta * t;
#endif

        if (aliveCount != index)
        {
            comp.particles[aliveCount] = std::move(particle);
        }
        ++aliveCount;
    }

    comp.particles.resize(aliveCount);
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
void ParticleSystem::EmitParticles(
    ParticleComponent& comp,
    float dt,
    const glm::vec3& worldPos,
    float emissionRate)
{
    if (emissionRate <= 0.0f || comp.maxParticles == 0) return;

    comp.timeSinceEmission += dt;
    const float emissionInterval = 1.0f / emissionRate;
    const glm::vec3 initialVelocity = comp.initialVelocity.ConvertToGLM();
#ifndef ANDROID
    const glm::vec4 startColor{
        comp.startColor.x, comp.startColor.y, comp.startColor.z, comp.startColorAlpha
    };
#endif

    while (comp.timeSinceEmission >= emissionInterval) 
    {
        comp.timeSinceEmission -= emissionInterval;

        if (comp.particles.size() >= comp.maxParticles) break;

        Particle p;
        p.position = worldPos;
        p.life = 1.0f;
#ifndef ANDROID
        p.size = comp.startSize;
        p.color = startColor;
#endif
        const float rotation = dist(rng) * kPi;
        p.rotationSinCos = { std::sin(rotation), std::cos(rotation) };

        // Add velocity randomness
        glm::vec3 randomVel(
            dist(rng) * comp.velocityRandomness,
            dist(rng) * comp.velocityRandomness,
            dist(rng) * comp.velocityRandomness
        );
        p.velocity = initialVelocity + randomVel;

        comp.particles.push_back(p);
    }
}

/******************************************************************************/
/*!
\fn         void ParticleSystem::UpdateInstanceBuffer(ParticleComponent& comp)
\brief      Uploads current particle data to GPU instance buffer

\details    Builds an array of ParticleInstanceData from active particles containing
            position, color, size, and precomputed rotation sine/cosine for each
            particle, then updates the instance VBO for use in instanced rendering.
            Early exits if no particles exist.

\param      comp - Reference to the particle component whose buffer needs updating
*/
/******************************************************************************/
void ParticleSystem::UpdateInstanceBuffer(ParticleComponent& comp)
{
	comp.hasParticleBounds = false;
    if (comp.particles.empty()) return;

    // Reuse static buffer to avoid per-frame heap allocation per emitter
    static std::vector<ParticleInstanceData> instanceData;
    instanceData.clear();
    instanceData.reserve(comp.particles.size());

	glm::vec3 boundsMin(std::numeric_limits<float>::max());
	glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
	bool boundsAreFinite = true;
#ifdef ANDROID
	const float sizeDelta = comp.endSize - comp.startSize;
#endif

    for (const auto& particle : comp.particles)
    {
        ParticleInstanceData data;
#ifdef ANDROID
        data.position[0] = particle.position.x;
        data.position[1] = particle.position.y;
        data.position[2] = particle.position.z;
        data.lifeUnorm = PackParticleUnorm(particle.life);
        data.padding = 0;
        data.rotationSinCos[0] = PackParticleSnorm(particle.rotationSinCos.x);
        data.rotationSinCos[1] = PackParticleSnorm(particle.rotationSinCos.y);
#else
        data.position = particle.position;
        data.color = particle.color;
        data.size = particle.size;
        data.rotationSinCos = particle.rotationSinCos;
#endif
        instanceData.push_back(data);

		// A rotated unit quad has a half-diagonal of sqrt(0.5). Expanding
		// equally on every axis remains conservative for any camera billboard.
#ifdef ANDROID
		const float particleSize =
			comp.startSize + sizeDelta * (1.0f - particle.life);
#else
		const float particleSize = particle.size;
#endif
		const float radius = glm::abs(particleSize) * 0.70710678118f;
		const glm::vec3 extent(radius);
		boundsMin = glm::min(boundsMin, particle.position - extent);
		boundsMax = glm::max(boundsMax, particle.position + extent);
		boundsAreFinite = boundsAreFinite &&
			std::isfinite(particle.position.x) &&
			std::isfinite(particle.position.y) &&
			std::isfinite(particle.position.z) &&
			std::isfinite(radius);
    }

	if (boundsAreFinite) {
		comp.particleBoundsMin = boundsMin;
		comp.particleBoundsMax = boundsMax;
		comp.hasParticleBounds = true;
	}

#ifdef __ANDROID__
	// The current positions are already packed and bounded. If the entire
	// emitter is outside the camera frustum, BuildRenderItem will reject it too;
	// avoid entering the GLES driver to upload data that cannot be drawn.
	if (comp.hasParticleBounds) {
		auto& graphics = GraphicsManager::GetInstance();
		if (graphics.IsFrustumCullingEnabled() &&
			!graphics.GetFrustum().IsBoxVisible(AABB(
				comp.particleBoundsMin,
				comp.particleBoundsMax))) {
			return;
		}
	}
#endif

    // UpdateData binds the VBO itself; an extra bind/unbind pair here only
    // adds driver traffic and is not required for later VAO-based drawing.
    comp.instanceVBO->UpdateData(instanceData.data(), instanceData.size() * sizeof(ParticleInstanceData));
}
