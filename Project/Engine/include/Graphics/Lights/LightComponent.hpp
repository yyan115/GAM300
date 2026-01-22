#pragma once
#include <glm/glm.hpp>
#include "Reflection/ReflectionBase.hpp"
#include <Math/Vector3D.hpp>
struct LightComponent 
{
	REFL_SERIALIZABLE

	Vector3D color = Vector3D::Ones();
	float intensity = 1.0f;
	bool enabled = true;

	LightComponent() = default;
};

struct DirectionalLightComponent : public LightComponent 
{
	REFL_SERIALIZABLE
	Vector3D direction = Vector3D(-0.2f, -1.0f, -0.3f);

	// Lighting properties
	Vector3D ambient = Vector3D::Ones();
	Vector3D diffuse = Vector3D::Ones();
	Vector3D specular = Vector3D::Ones()*0.5f;

	DirectionalLightComponent() = default;
};

struct PointLightComponent : public LightComponent 
{
	REFL_SERIALIZABLE
	// Attenuation
	float constant = 1.0f;
	float linear = 0.09f;
	float quadratic = 0.032f;

	
	Vector3D ambient = Vector3D(0.05f, 0.05f, 0.05f);
	Vector3D diffuse = Vector3D(0.8f, 0.8f, 0.8f);
	Vector3D specular = Vector3D::Ones();

	bool castShadows = false;

	PointLightComponent() = default;
};

struct SpotLightComponent : public LightComponent 
{
	REFL_SERIALIZABLE
	Vector3D direction = Vector3D(0.0f, 0.0f, -1.0f);

	// Cone angles (in radians for direct use)
	float cutOff = 0.976f; // cos(12.5 degrees)
	float outerCutOff = 0.966f; // cos(15 degrees)

	// Attenuation
	float constant = 1.0f;
	float linear = 0.09f;
	float quadratic = 0.032f;

	// Lighting properties
	Vector3D ambient = Vector3D::Zero();
	Vector3D diffuse = Vector3D::Ones();
	Vector3D specular = Vector3D::Ones();

	SpotLightComponent() = default;
};