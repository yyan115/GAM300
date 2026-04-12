#pragma once
#include "Reflection/ReflectionBase.hpp"
#include <glm/glm.hpp>

class IRenderComponent {
public:
	REFL_SERIALIZABLE

	bool isVisible = true;
	int renderOrder = 100;
	bool excludeFromPostProcess = false; // Runtime-only: render after post-processing

	// Per-entity bloom emission (set from BloomComponent)
	glm::vec3 bloomColor = glm::vec3(0.0f);
	float bloomIntensity = 0.0f;

	// Per-entity brightness multiplier (applied to lighting result in fragment shader)
	float brightnessBoost = 1.0f;

	IRenderComponent() = default;
	virtual ~IRenderComponent() = default;

};