#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include "../include/Graphics/Material.hpp"
#include "Model.h"
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include "Math/Matrix4x4.hpp"
#include "Utilities/GUID.hpp"

class Shader;
class Camera;

class ModelRenderComponent : public IRenderComponent {
public:
	// Serialize these.
	REFL_SERIALIZABLE
	GUID_128 modelGUID{};
	GUID_128 shaderGUID{};
	GUID_128 materialGUID{};
	Matrix4x4 transform;
	bool isVisible = true;

	// Don't serialize these.
	std::shared_ptr<Model> model;
	std::shared_ptr<Shader> shader;
	// Single material for the entire model (like Unity)
	std::shared_ptr<Material> material;

	ModelRenderComponent(GUID_128 m_GUID, GUID_128 s_GUID, GUID_128 mat_GUID)
		: modelGUID(m_GUID), shaderGUID(s_GUID), materialGUID(mat_GUID), transform(), isVisible(true) { }
	ModelRenderComponent() = default;
	~ModelRenderComponent() = default;

	// Get material for a specific mesh (returns entity material if set, otherwise model default)
	std::shared_ptr<Material> GetMaterial(size_t meshIndex) const {
		if (material) {
			return material;
		}
		if (model && meshIndex < model->meshes.size()) {
			return model->meshes[meshIndex].material;
		}
		return nullptr;
	}

	// Set the material for the entire model
	void SetMaterial(std::shared_ptr<Material> mat) {
		material = mat;
	}

	//int GetRenderOrder() const override { return 100; }
	//bool IsVisible() const override { return isVisible && model && shader; }
};
