#pragma once
#include <cstddef>
#include <limits>
#include <cstdint>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../OpenGL.h"
#include "Engine.h"
#include "Graphics/VBO.h"

class Model;
class Material;
class Shader;

struct InstanceData {
	#ifdef ANDROID
	// Affine model basis columns with translation packed into each .w. Mobile
	// shaders reconstruct the implicit [0, 0, 0, 1] row, saving one attribute.
	float modelColumns[12];
	std::uint16_t normalMatrixColumns[9];
	std::uint16_t bloomData[4];
	std::uint16_t padding = 0;
	std::uint32_t lightMask;
	#else
	glm::mat4 modelMatrix;
	glm::vec4 bloomData;  // xyz = bloomColor, w = bloomIntensity
	float normalMatrixColumns[9];
	std::uint32_t lightMask;
	#endif
};

#ifdef ANDROID
static_assert(sizeof(InstanceData) == 80, "Android instance data must stay tightly packed");
static_assert(offsetof(InstanceData, lightMask) == 76);
#else
static_assert(sizeof(InstanceData) == 120, "InstanceData must stay tightly packed");
#endif

class ENGINE_API InstanceBatch {
public:
	InstanceBatch();
	~InstanceBatch();

	InstanceBatch(const InstanceBatch&) = delete;
	InstanceBatch& operator=(const InstanceBatch&) = delete;
	InstanceBatch(InstanceBatch&& other) noexcept;
	InstanceBatch& operator=(InstanceBatch&& other) noexcept;

	void Initialize(
		const std::shared_ptr<Model>& model,
		const std::shared_ptr<Material>& material,
		const std::shared_ptr<Shader>& shader);
	void Clear();

	void AddInstance(
		const glm::mat4& modelMatrix,
		const glm::vec3& bloomColor = glm::vec3(0.0f),
		float bloomIntensity = 0.0f,
		std::uint32_t lightMask = 0xFFFFFFFFu,
		float cameraDistanceSq = 0.0f);

	void Render();

	void RenderDepthOnly(const glm::mat4& lightSpaceMatrix);

	// Accessors
	size_t GetInstanceCount() const { return m_instanceCount; }
	bool IsEmpty() const { return m_instanceCount == 0; }
	void FinalizeDepthBucket();

	Model* GetModel() const { return m_model.get(); }
	Material* GetMaterial() const { return m_material.get(); }
	Shader* GetShader() const { return m_shader.get(); }
	int GetDepthBucket() const { return m_depthBucket; }
	bool HasBloomEmission() const { return m_hasBloomEmission; }

	// For sorting/comparison
	size_t GetSortKey() const;

	void Prewarm(size_t expectedInstanceCount = 0);

private:
	void UpdateInstanceBuffer();

	std::shared_ptr<Model> m_model;
	std::shared_ptr<Material> m_material;
	std::shared_ptr<Shader> m_shader;
	
	std::vector<InstanceData> m_instances;
	size_t m_instanceCount = 0;

	VBO m_instanceVBO;
	size_t m_bufferCapacity = 0;
	bool m_bufferDirty = true;
	bool m_initialized = false;
	int m_depthBucket = std::numeric_limits<int>::max();
	float m_minCameraDistanceSq = std::numeric_limits<float>::infinity();
	bool m_hasBloomEmission = false;

	static constexpr size_t INITIAL_CAPACITY = 16;
	static constexpr size_t GROWTH_FACTOR = 2;
};
