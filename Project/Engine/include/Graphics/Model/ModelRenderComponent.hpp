#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include "../include/Graphics/Material.hpp"
#include "Model.h"
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include "Math/Matrix4x4.hpp"
#include "Utilities/GUID.hpp"
#include <Animation/Animator.hpp>

class Shader;
class Camera;

class ModelRenderComponent : public IRenderComponent {
public:
	// Serialize these.
	REFL_SERIALIZABLE
	bool overrideFromPrefab = false;
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

	Vector3D CalculateModelHalfExtent(const Model& _model) {
		Vector3D minPt(FLT_MAX, FLT_MAX, FLT_MAX);
		Vector3D maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (const auto& mesh : _model.meshes) {
			for (const auto& vertex : mesh.vertices) {
				if (vertex.position.x < minPt.x) minPt.x = vertex.position.x;
				if (vertex.position.y < minPt.y) minPt.y = vertex.position.y;
				if (vertex.position.z < minPt.z) minPt.z = vertex.position.z;

				if (vertex.position.x > maxPt.x) maxPt.x = vertex.position.x;
				if (vertex.position.y > maxPt.y) maxPt.y = vertex.position.y;
				if (vertex.position.z > maxPt.z) maxPt.z = vertex.position.z;
			}
		}

		Vector3D halfExtent;
		halfExtent.x = (maxPt.x - minPt.x) * 0.5f;
		halfExtent.y = (maxPt.y - minPt.y) * 0.5f;
		halfExtent.z = (maxPt.z - minPt.z) * 0.5f;

		return halfExtent;
	}

	float CalculateModelRadius(const Model& _model)
	{
		Vector3D minPt(FLT_MAX, FLT_MAX, FLT_MAX);
		Vector3D maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		// 1️⃣ Find the bounding box of the model
		for (const auto& mesh : _model.meshes)
		{
			for (const auto& vertex : mesh.vertices)
			{
				const auto& p = vertex.position;

				if (p.x < minPt.x) minPt.x = p.x;
				if (p.y < minPt.y) minPt.y = p.y;
				if (p.z < minPt.z) minPt.z = p.z;

				if (p.x > maxPt.x) maxPt.x = p.x;
				if (p.y > maxPt.y) maxPt.y = p.y;
				if (p.z > maxPt.z) maxPt.z = p.z;
			}
		}

		// 2️⃣ Compute the center of the bounding box
		Vector3D center = (maxPt + minPt) * 0.5f;

		// 3️⃣ Compute the maximum distance from center to any vertex
		float radius = 0.0f;
		for (const auto& mesh : _model.meshes)
		{
			for (const auto& vertex : mesh.vertices)
			{
				Vector3D diff = Vector3D::ConvertGLMToVector3D(vertex.position) - center;
				float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
				if (distSq > radius * radius)
					radius = std::sqrt(distSq);
			}
		}

		return radius;
	}



	//int GetRenderOrder() const override { return 100; }
	//bool IsVisible() const override { return isVisible && model && shader; }


	Animator* animator = nullptr;
	bool HasAnimation() const { return animator != nullptr; }
	void SetAnimator(Animator* anim) { animator = anim; }

};
