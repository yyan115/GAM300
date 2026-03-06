#pragma once
#include "Reflection/ReflectionBase.hpp"
#include <glm/glm.hpp>
#include <cstdint>

class IRenderComponent {
public:
	REFL_SERIALIZABLE

	bool isVisible = true;
	int renderOrder = 100;
	bool excludeFromPostProcess = false; // Runtime-only: render after post-processing

	// ECS entity that owns this render item.
	// Set by each system before submitting to GraphicsManager.
	// UINT32_MAX means unset (occlusion culling will skip this item).
	uint32_t entityId = UINT32_MAX;

	// Per-entity bloom emission (set from BloomComponent)
	glm::vec3 bloomColor = glm::vec3(0.0f);
	float bloomIntensity = 0.0f;

	IRenderComponent() = default;
	virtual ~IRenderComponent() = default;

};