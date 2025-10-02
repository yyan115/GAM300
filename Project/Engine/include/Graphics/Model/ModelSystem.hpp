#pragma once
#include <memory>
#include <vector>
#include "ECS/System.hpp"
#include "Model.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/ShaderClass.h"

class ModelSystem : public System {
public:
    ModelSystem() = default;
    ~ModelSystem() = default;

    bool Initialise();
    void Update();
    void Shutdown();
};