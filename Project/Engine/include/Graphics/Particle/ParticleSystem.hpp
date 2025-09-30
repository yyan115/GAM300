#pragma once
#pragma once
#include "ECS/System.hpp"
#include <random>
#include "ParticleComponent.hpp"

class ParticleSystem : public System {
public:
    bool Initialise();
    void Update();
    void Shutdown();

private:
    void UpdateParticles(ParticleComponent& comp, float dt);
    void EmitParticles(ParticleComponent& comp, float dt);
    void RemoveDeadParticles(ParticleComponent& comp);

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };
};