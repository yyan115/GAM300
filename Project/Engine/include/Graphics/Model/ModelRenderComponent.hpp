#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include "../include/Graphics/Material.hpp"
#include <memory>
#include <glm/glm.hpp>
#include <vector>

class Model;
class Shader;
class Camera;

class ModelRenderComponent : public IRenderComponent {
public:
	std::shared_ptr<Model> model;
	std::shared_ptr<Shader> shader;
	// Single material for the entire model (like Unity)
	std::shared_ptr<Material> material;
	glm::mat4 transform;
	bool isVisible = true;

	ModelRenderComponent(std::shared_ptr<Model> m, std::shared_ptr<Shader> s)
		: model(std::move(m)), shader(std::move(s)), transform(), isVisible(true) {}
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
