/* Start Header ************************************************************************/
/*!
\file       ParticleSystem.hpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu
\date       Oct 2, 2025
\brief      Header file for the ParticleSystem class, an ECS system that manages
            particle emission, physics simulation, and GPU-instanced rendering.
            Provides interface for initializing, updating, and shutting down
            particle effects with configurable emission rates and physics properties.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "ECS/System.hpp"
#include <cstdint>
#include <random>
#include "ParticleComponent.hpp"
#include "ParticleRenderItem.hpp"

/******************************************************************************/
/*!
\struct     ParticleInstanceData
\brief      GPU instance data structure for a single particle

\details    Contains the per-particle data needed for instanced rendering.
            Android interpolates emitter color and size in the vertex shader,
            so its compact stream contains position, life, and rotation only.
*/
/******************************************************************************/
#ifdef ANDROID
struct ParticleInstanceData {
    float position[3];
    std::uint16_t lifeUnorm;
    std::uint16_t padding;
    std::int16_t rotationSinCos[2];
};
static_assert(sizeof(ParticleInstanceData) == 20);
#else
struct ParticleInstanceData {
    glm::vec3 position;
    glm::vec4 color;
    float size;
    glm::vec2 rotationSinCos;
};
#endif

/******************************************************************************/
/*!
\class      ParticleSystem
\brief      ECS system for managing particle effects with GPU instancing

\details    Handles initialization of OpenGL buffers, particle emission based on
            configurable rates, physics updates with gravity and velocity,
            lifetime management, and efficient GPU buffer updates for rendering.
            Uses random number generation for velocity variation.
*/
/******************************************************************************/
class ParticleSystem : public System {
public:
    bool Initialise(bool forceInit = false);
    bool InitialiseParticles(bool forceInit);
    void Update();
    void Shutdown();

private:
    void InitializeParticleComponent(ParticleComponent& particleComp);
    void UpdateParticles(ParticleComponent& comp, float dt);
    void EmitParticles(
        ParticleComponent& comp,
        float dt,
        const glm::vec3& worldPos,
        float emissionRate);
    void UpdateInstanceBuffer(ParticleComponent& comp);

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };
    std::vector<ParticleRenderItem> renderSnapshots;
    bool particleSystemInitialised = false;
};
