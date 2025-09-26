#pragma once
#include <glm/glm.hpp>

struct LightComponent {
	glm::vec3 color = glm::vec3(1.0f);
	float intensity = 1.0f;
	bool enabled = true;

	LightComponent() = default;
};

struct DirectionalLightComponent : public LightComponent {
	glm::vec3 direction = glm::vec3(-0.2f, -1.0f, -0.3f);

	// Lighting properties
	glm::vec3 ambient = glm::vec3(1.0f);
	glm::vec3 diffuse = glm::vec3(1.0f);
	glm::vec3 specular = glm::vec3(0.5f);

	DirectionalLightComponent() = default;
};

struct PointLightComponent : public LightComponent {
	// Attenuation
	float constant = 1.0f;
	float linear = 0.09f;
	float quadratic = 0.032f;

	
	glm::vec3 ambient = glm::vec3(0.05f);
	glm::vec3 diffuse = glm::vec3(0.8f);
	glm::vec3 specular = glm::vec3(1.0f);

	PointLightComponent() = default;
};

struct SpotLightComponent : public LightComponent {
	glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);

	// Cone angles (in radians for direct use)
	float cutOff = 0.976f; // cos(12.5 degrees)
	float outerCutOff = 0.966f; // cos(15 degrees)

	// Attenuation
	float constant = 1.0f;
	float linear = 0.09f;
	float quadratic = 0.032f;

	// Lighting properties
	glm::vec3 ambient = glm::vec3(0.0f);
	glm::vec3 diffuse = glm::vec3(1.0f);
	glm::vec3 specular = glm::vec3(1.0f);

	SpotLightComponent() = default;
};