#pragma once
#pragma once
#include "ECS/System.hpp"
#include <random>
#include "ParticleComponent.hpp"

struct ParticleInstanceData {
    glm::vec3 position;
    glm::vec4 color;
    float size;
    float rotation;
};

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