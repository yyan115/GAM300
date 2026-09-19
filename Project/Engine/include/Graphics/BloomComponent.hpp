#pragma once
#include <glm/glm.hpp>
#include "Reflection/ReflectionBase.hpp"

struct BloomComponent {
    REFL_SERIALIZABLE

    bool enabled = true;                      // Toggle bloom on/off
    glm::vec3 bloomColor = glm::vec3(1.0f);  // Glow color
    float bloomIntensity = 1.0f;              // Emission multiplier (pushes into HDR range)

    // For scripts, which cannot hand the engine a glm::vec3
    void SetColor(float r, float g, float b) { bloomColor = glm::vec3(r, g, b); }
};
