#pragma once
#include "Reflection/ReflectionBase.hpp"
#include <glm/glm.hpp>

enum class RenderComponentKind {
	Unknown,
	Model,
	Text,
	TextRenderItem,
	Sprite,
	SpriteRenderItem,
	DebugDraw,
	Particle,
	ParticleRenderItem,
	Fog
};

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

	virtual RenderComponentKind GetRenderKind() const { return RenderComponentKind::Unknown; }

};
