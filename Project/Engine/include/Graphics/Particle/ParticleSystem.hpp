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
#include <random>
#include "ParticleComponent.hpp"

/******************************************************************************/
/*!
\struct     ParticleInstanceData
\brief      GPU instance data structure for a single particle

\details    Contains all per-particle data needed for instanced rendering:
            position in world space, RGBA color with alpha, size scalar,
            and rotation in degrees. Tightly packed for efficient GPU upload.
*/
/******************************************************************************/
struct ParticleInstanceData {
    glm::vec3 position;
    glm::vec4 color;
    float size;
    float rotation;
};

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
    bool Initialise();
    void Update();
    void Shutdown();

private:
    void InitializeParticleComponent(ParticleComponent& particleComp);
    void UpdateParticles(ParticleComponent& comp, float dt);
    void EmitParticles(ParticleComponent& comp, float dt);
    void RemoveDeadParticles(ParticleComponent& comp);
    void UpdateInstanceBuffer(ParticleComponent& comp);

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };
};