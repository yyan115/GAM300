/* Start Header ************************************************************************/
/*!
\file       ParticleSystem.cpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu (90%)
\date       Oct 2, 2025
\brief      

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

bool ParticleSystem::Initialise() 
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    for (const auto& entity : entities)
    {
        auto& particleComp = ecsManager.GetComponent<ParticleComponent>(entity);

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

        particleComp.particleVAO->Unbind();
        particleComp.quadVBO->Unbind();
        particleComp.quadEBO->Unbind();

        // Reserve particle pool
        particleComp.particles.reserve(particleComp.maxParticles);

        ENGINE_PRINT("[ParticleSystem] Initialized particle emitter for entity with ", particleComp.maxParticles, " max particles\n");
    }

    return true;
}

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

    particleComp.particleVAO->Unbind();
    particleComp.quadVBO->Unbind();
    particleComp.quadEBO->Unbind();

    particleComp.particles.reserve(particleComp.maxParticles);
}

void ParticleSystem::Update()
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    GraphicsManager& gfxManager = GraphicsManager::GetInstance();
    float dt = TimeManager::GetDeltaTime();

    for (const auto& entity : entities)
    {
        auto& particleComp = ecsManager.GetComponent<ParticleComponent>(entity);

        // Initialize if not already done
        if (!particleComp.particleVAO) // In case new particle system is added on the fly
        {
            InitializeParticleComponent(particleComp);
        }

        if (!particleComp.isVisible) continue;

        // Update particle physics
        UpdateParticles(particleComp, dt);

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
}

void ParticleSystem::UpdateParticles(ParticleComponent& comp, float dt)
{
    for (auto& particle : comp.particles) 
    {
        // Update physics
        particle.velocity += comp.gravity * dt;
        particle.position += particle.velocity * dt;

        // Update life
        particle.life -= dt / comp.particleLifetime;

        // Interpolate properties based on life
        float t = 1.0f - particle.life;
        particle.size = glm::mix(comp.startSize, comp.endSize, t);
        particle.color = glm::mix(comp.startColor, comp.endColor, t);
    }
}

void ParticleSystem::EmitParticles(ParticleComponent& comp, float dt)
{
    comp.timeSinceEmission += dt;
    float emissionInterval = 1.0f / comp.emissionRate;

    while (comp.timeSinceEmission >= emissionInterval) 
    {
        comp.timeSinceEmission -= emissionInterval;

        if (comp.particles.size() >= comp.maxParticles) break;

        Particle p;
        p.position = comp.emitterPosition;
        p.life = 1.0f;
        p.size = comp.startSize;
        p.color = comp.startColor;
        p.rotation = dist(rng) * 360.0f;

        // Add velocity randomness
        glm::vec3 randomVel(
            dist(rng) * comp.velocityRandomness,
            dist(rng) * comp.velocityRandomness,
            dist(rng) * comp.velocityRandomness
        );
        p.velocity = comp.initialVelocity + randomVel;

        comp.particles.push_back(p);
    }
}

void ParticleSystem::RemoveDeadParticles(ParticleComponent& comp)
{
    comp.particles.erase(
        std::remove_if(comp.particles.begin(), comp.particles.end(),
            [](const Particle& p) { return p.life <= 0.0f; }),
        comp.particles.end()
    );
}

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
