#pragma once
#include <glm/glm.hpp>
#include "Reflection/ReflectionBase.hpp"

struct BloomComponent {
    REFL_SERIALIZABLE

    bool enabled = true;                      // Toggle bloom on/off
    glm::vec3 bloomColor = glm::vec3(1.0f);  // Glow color
    float bloomIntensity = 1.0f;              // Emission multiplier (pushes into HDR range)
};
