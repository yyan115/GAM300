#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include <memory>
#include <glm/glm.hpp>
#include "Math/Matrix4x4.hpp"
#include "Utilities/GUID.hpp"

class Model;
class Shader;
class Camera;

class ModelRenderComponent : public IRenderComponent {
public:
	// Serialize these.
	REFL_SERIALIZABLE
	GUID_128 modelGUID{};
	GUID_128 shaderGUID{};
	Matrix4x4 transform;
	bool isVisible = true;

	// Don't serialize these.
	std::shared_ptr<Model> model;
	std::shared_ptr<Shader> shader;

	ModelRenderComponent(GUID_128 m_GUID, GUID_128 s_GUID)
		: modelGUID(m_GUID), shaderGUID(s_GUID), transform(), isVisible(true) { }
	ModelRenderComponent() = default;
	~ModelRenderComponent() = default;
};
