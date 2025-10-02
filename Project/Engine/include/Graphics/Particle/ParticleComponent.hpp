#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <glm/glm.hpp>
#include <vector>

class Texture;
class Shader;
class VAO;
class VBO;
class EBO;

struct Particle {
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec4 color;

	float life;
	float size;
	float rotation;
};

class ParticleComponent : public IRenderComponent {
public:
    // Emitter properties
    glm::vec3 emitterPosition;
    float emissionRate = 10.0f;
    int maxParticles = 1000;

    // Particle properties
    float particleLifetime = 2.0f;
    float startSize = 0.1f;
    float endSize = 0.0f;
    glm::vec4 startColor = glm::vec4(1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

    // Physics
    glm::vec3 gravity = glm::vec3(0.0f, -9.8f, 0.0f);
    float velocityRandomness = 1.0f;
    glm::vec3 initialVelocity = glm::vec3(0.0f, 1.0f, 0.0f);

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