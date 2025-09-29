#pragma once
#include <memory>
#include <vector>
#include "ECS/System.hpp"
#include "SpriteRenderComponent.hpp"

class VAO;
class VBO;
class EBO;

class SpriteSystem : public System 
{
public:
    SpriteSystem() = default;
    ~SpriteSystem() = default;

    bool Initialise();
    void Update();
    void Shutdown();

private:
    void InitializeSpriteQuad();
    void CleanupSpriteQuad();

    std::unique_ptr<VAO> spriteVAO;
    std::unique_ptr<VBO> spriteVBO;
    std::unique_ptr<EBO> spriteEBO;
    bool spriteQuadInitialized = false;
};
