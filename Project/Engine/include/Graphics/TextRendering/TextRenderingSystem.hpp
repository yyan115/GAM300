#pragma once
#include "ECS/System.hpp"

class TextRenderingSystem : public System {
public:
    TextRenderingSystem() = default;
    ~TextRenderingSystem() = default;

    bool Initialise();
    void Update();
    void Shutdown();
};