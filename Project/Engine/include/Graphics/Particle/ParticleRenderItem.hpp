#pragma once

#include "Graphics/IRenderComponent.hpp"

#include <cstddef>
#include <memory>

class EBO;
class Shader;
class Texture;
class VAO;

// Lightweight frame snapshot. Particle instance data already lives in the GPU
// buffer, so the render queue must not copy the simulation's particle vector.
class ParticleRenderItem final : public IRenderComponent {
public:
    std::shared_ptr<Texture> particleTexture;
    std::shared_ptr<Shader> particleShader;
    VAO* particleVAO = nullptr;
    EBO* quadEBO = nullptr;
    std::size_t particleCount = 0;
    bool additiveBlending = false;

    RenderComponentKind GetRenderKind() const override { return RenderComponentKind::ParticleRenderItem; }
};
