/* Start Header ************************************************************************/
/*!
\file       ParticleComponent.hpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu
\date       Oct 2, 2025
\brief      Header file defining particle-related data structures and component.
            Contains the Particle struct for individual particle properties and
            ParticleComponent class for ECS integration with configurable emitter
            settings, physics parameters, and rendering resources.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <Math/Vector3D.hpp>

// Forward Declarations
class Texture;
class Shader;
class VAO;
class VBO;
class EBO;

/******************************************************************************/
/*!
\struct     Particle
\brief      Runtime data structure for a single particle instance

\details    Stores all properties needed to simulate and render an individual
            particle including position, velocity, color with alpha, remaining
            lifetime (0.0 = dead, 1.0 = newly spawned), size, and rotation.
*/
/******************************************************************************/
struct Particle {
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec4 color;

	float life;
	float size;
	float rotation;
};

/******************************************************************************/
/*!
\class      ParticleComponent
\brief      ECS component for particle emitter configuration and runtime data

\details    Inherits from IRenderComponent to integrate with the rendering system.
            Contains all configurable properties for particle emission (rate, max count),
            appearance (size, color interpolation), physics (gravity, velocity),
            and runtime OpenGL resources (VAO, VBOs, EBO) for GPU-instanced rendering.
            Maintains a pool of active particles updated each frame.
*/
/******************************************************************************/
class ParticleComponent : public IRenderComponent {
public:
    REFL_SERIALIZABLE
    GUID_128 textureGUID{};

    // Emitter properties
    Vector3D emitterPosition;
    float emissionRate = 10.0f;
    int maxParticles = 1000;

    // Particle properties
    float particleLifetime = 2.0f;
    float startSize = 0.1f;
    float endSize = 0.0f;
    //glm::vec4 startColor = glm::vec4(1.0f);
    //glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    // To be serialized
    Vector3D startColor = Vector3D{ 1, 1, 1 };
    float startColorAlpha = 1.0f;
    Vector3D endColor = Vector3D{ 1, 1, 1 };
    float endColorAlpha = 1.0f;

    // Physics
    Vector3D gravity = Vector3D(0.0f, -9.8f, 0.0f);
    float velocityRandomness = 1.0f;
    Vector3D initialVelocity = Vector3D(0.0f, 1.0f, 0.0f);

    // Runtime data (don't serialize)
    std::vector<Particle> particles;
    std::shared_ptr<Texture> particleTexture;
    std::shared_ptr<Shader> particleShader;

    VAO* particleVAO = nullptr;
    VBO* quadVBO = nullptr;      // Vertex data for quad
    EBO* quadEBO = nullptr;      // Index data for quad
    VBO* instanceVBO = nullptr;  // Instance data (per-particle)

    float timeSinceEmission = 0.0f;
    bool isEmitting = true;

    ParticleComponent() = default;
    ~ParticleComponent() = default;
};