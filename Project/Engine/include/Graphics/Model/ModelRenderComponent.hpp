#pragma once
#include "../include/Graphics/IRenderComponent.hpp"
#include "../include/Graphics/Material.hpp"
#include "Model.h"
#include <memory>
#include <cstdint>
#include <limits>
#include <glm/glm.hpp>
#include <vector>
#include "Math/Matrix4x4.hpp"
#include "Utilities/GUID.hpp"
#include <Animation/Animator.hpp>

class Shader;
class Camera;

class ModelRenderComponent : public IRenderComponent {
public:
	struct RenderSnapshotTag {};

	// Serialize these.
	REFL_SERIALIZABLE
	bool overrideFromPrefab = false;
	GUID_128 modelGUID{};
	GUID_128 shaderGUID{};
	GUID_128 materialGUID{};
	Matrix4x4 transform;
	bool isVisible = true;
	bool childBonesSaved = false;
	// Depth offset to fix z-fighting on coplanar geometry.
	// factor/units match glPolygonOffset(factor, units).
	// Negative values push towards the camera (render on top).
	bool depthOffset = false;
	float depthOffsetFactor = -1.0f;
	float depthOffsetUnits = -1.0f;

	// Don't serialize these.
	float distanceFadeOpacity = 1.0f; // Set per-frame by ModelSystem distance fade
	std::uint32_t lightMask = 0xFFFFFFFFu;
	std::shared_ptr<Model> model;
	std::shared_ptr<Shader> shader;
	// Single material for the entire model (like Unity)
	std::shared_ptr<Material> material;
	// Incremented once after an animator finishes writing a complete skin pose.
	// Render preparation uses this to retain exact bounds for frozen/cull-skipped
	// animations instead of transforming every bone bound again each frame.
	std::uint64_t bonePoseRevision = 0;
#ifdef __ANDROID__
	std::size_t bonePaletteOffset =
		std::numeric_limits<std::size_t>::max();
#endif

	// Static authored models retain these derived values until their transform
	// or model resource changes. Animated bounds bypass this cache.
	const Model* cachedBoundsModel = nullptr;
	std::uint64_t cachedBoundsWorldRevision =
		std::numeric_limits<std::uint64_t>::max();
	AABB cachedWorldBounds;
#ifdef __ANDROID__
	const Model* cachedSkinnedBoundsModel = nullptr;
	std::uint64_t cachedSkinnedBoundsPoseRevision =
		std::numeric_limits<std::uint64_t>::max();
	AABB cachedSkinnedLocalBounds;
	bool cachedSkinnedBoundsValid = false;
	const Model* cachedLightMaskModel = nullptr;
	std::uint64_t cachedLightMaskWorldRevision =
		std::numeric_limits<std::uint64_t>::max();
	std::uint64_t cachedLightingRevision =
		std::numeric_limits<std::uint64_t>::max();
	std::uint32_t cachedLightMask = 0xFFFFFFFFu;
#endif

	ModelRenderComponent(GUID_128 m_GUID, GUID_128 s_GUID, GUID_128 mat_GUID)
		: modelGUID(m_GUID), shaderGUID(s_GUID), materialGUID(mat_GUID), transform(), isVisible(true) {}

	ModelRenderComponent() = default;

	// Render submissions do not need the model hierarchy lookup map. Static
	// models also do not need the component's placeholder bone matrices.
	ModelRenderComponent(const ModelRenderComponent& other, RenderSnapshotTag) {
		UpdateRenderSnapshot(other);
	}

	void UpdateRenderSnapshot(const ModelRenderComponent& other) {
		static_cast<IRenderComponent&>(*this) = static_cast<const IRenderComponent&>(other);
		overrideFromPrefab = other.overrideFromPrefab;
		modelGUID = other.modelGUID;
		shaderGUID = other.shaderGUID;
		materialGUID = other.materialGUID;
		transform = other.transform;
		isVisible = other.isVisible;
		childBonesSaved = other.childBonesSaved;
		depthOffset = other.depthOffset;
		depthOffsetFactor = other.depthOffsetFactor;
		depthOffsetUnits = other.depthOffsetUnits;
		distanceFadeOpacity = other.distanceFadeOpacity;
		lightMask = other.lightMask;
		model = other.model;
		shader = other.shader;
		material = other.material;
		bonePoseRevision = other.bonePoseRevision;
#ifdef __ANDROID__
		bonePaletteOffset = std::numeric_limits<std::size_t>::max();
#endif
		animator = other.animator;
		renderBoneMatrices = nullptr;

		if (HasAnimation() && model && !model->mBoneInfoMap.empty()) {
			// Animation has finished before render preparation starts, and ECS
			// component addresses remain stable for the frame. Refer to its matrix
			// array instead of copying up to 100 matrices for every animated draw.
			mFinalBoneMatrices.clear();
			renderBoneMatrices = &other.mFinalBoneMatrices;
		}
		else if (model && !model->mBoneInfoMap.empty()) {
			// Manual-bone rendering updates the snapshot below, so it still needs
			// an independent writable array.
			mFinalBoneMatrices = other.mFinalBoneMatrices;
		}
		else {
			mFinalBoneMatrices.clear();
		}
	}

	~ModelRenderComponent() = default;

	RenderComponentKind GetRenderKind() const override { return RenderComponentKind::Model; }

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


	Vector3D CalculateCenter(const Model& _model)
	{
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
		Vector3D center;
		center.x = (minPt.x + maxPt.x) * 0.5f;
		center.y = (minPt.y + maxPt.y) * 0.5f;
		center.z = (minPt.z + maxPt.z) * 0.5f;
		return center;
	}


	//int GetRenderOrder() const override { return 100; }
	//bool IsVisible() const override { return isVisible && model && shader; }

	std::vector<glm::mat4> mFinalBoneMatrices;
	const std::vector<glm::mat4>& GetRenderBoneMatrices() const noexcept {
		return renderBoneMatrices ? *renderBoneMatrices : mFinalBoneMatrices;
	}
	std::map<std::string, Entity> boneNameToEntityMap;

	Animator* animator = nullptr;
	bool HasAnimation() const { return animator != nullptr; }
	void SetAnimator(Animator* anim) { animator = anim; }

	void SetVisible(bool v) { isVisible = v; }
	bool IsVisible() const { return isVisible; }

private:
	const std::vector<glm::mat4>* renderBoneMatrices = nullptr;
};
