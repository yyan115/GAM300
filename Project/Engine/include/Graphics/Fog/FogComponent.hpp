#pragma once
#include "Graphics/IRenderComponent.hpp"
#include <glm/glm.hpp>
#include <Math/Vector3D.hpp>
#include "Math/Matrix4x4.hpp"
#include "Utilities/GUID.hpp"

class Texture;
class Shader;
class VAO;
class VBO;
class EBO;

enum class FogShape {
	BOX,
	SPHERE,
	CYLINDER
};

class FogVolumeComponent : public IRenderComponent {
public:
	REFL_SERIALIZABLE
	FogShape shape = FogShape::BOX;

	Vector3D fogColor = Vector3D(0.7f, 0.7f, 0.7f);
	float fogColorAlpha = 1.0f;
	float density = 0.5f;
	float opacity = 0.6f;

	float scrollSpeedX = 0.02f;
	float scrollSpeedY = 0.01f;
	float noiseScale = 1.0f;
	float noiseStrength = 0.5f;

	bool useHeightFade = true;
	float heightFadeStart = 0.0f;
	float heightFadeEnd = 1.0f;

	float edgeSoftness = 0.3f;

	GUID_128 noiseTextureGUID{};

	std::shared_ptr<Shader> fogShader;
	std::shared_ptr<Texture> noiseTexture;
	std::string noiseTexturePath;   // For inspector display

	VAO* fogVAO = nullptr;
	VBO* fogVBO = nullptr;
	EBO* fogEBO = nullptr;

	Matrix4x4 worldTransform;


	FogVolumeComponent() {
		renderOrder = 500;  // After opaque geometry, before UI
	}
	~FogVolumeComponent() = default;

};