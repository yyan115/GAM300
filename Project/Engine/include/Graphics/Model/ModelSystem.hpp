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

    struct CullingStats {
        int totalObjects = 0;
        int culledObjects = 0;
        int renderedObjects = 0;

        void Reset()
        {
            totalObjects = 0;
            culledObjects = 0;
            renderedObjects = 0;
        }

        float GetCulledPercentage() const 
        {
            if (totalObjects == 0) return 0.0f;
            return (culledObjects / (float)totalObjects) * 100.0f;
        }
    };

    const CullingStats& GetCullingStats() const { return cullingStats; }

private:
    CullingStats cullingStats;
};