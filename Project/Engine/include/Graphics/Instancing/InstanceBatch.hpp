#pragma once
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
	glm::mat4 modelMatrix;
	glm::mat4 normalMatrix;
};

class ENGINE_API InstanceBatch {
public:
	InstanceBatch();
	~InstanceBatch();

	InstanceBatch(const InstanceBatch&) = delete;
	InstanceBatch& operator=(const InstanceBatch&) = delete;
	InstanceBatch(InstanceBatch&& other) noexcept;
	InstanceBatch& operator=(InstanceBatch&& other) noexcept;

	void Initialize(std::shared_ptr<Model> model, std::shared_ptr<Material> material, std::shared_ptr<Shader> shader);
	void Clear();

	void AddInstance(const glm::mat4& modelMatrix);

	void Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

	void RenderDepthOnly(const glm::mat4& lightSpaceMatrix);

	// Accessors
	size_t GetInstanceCount() const { return m_instances.size(); }
	bool IsEmpty() const { return m_instances.empty(); }

	Model* GetModel() const { return m_model.get(); }
	Material* GetMaterial() const { return m_material.get(); }
	Shader* GetShader() const { return m_shader.get(); }

	// For sorting/comparison
	size_t GetSortKey() const;

private:
	void CreateInstanceBuffer();
	void UpdateInstanceBuffer();

	std::shared_ptr<Model> m_model;
	std::shared_ptr<Material> m_material;
	std::shared_ptr<Shader> m_shader;
	
	std::vector<InstanceData> m_instances;

	VBO m_instanceVBO;
	size_t m_bufferCapacity = 0;
	bool m_bufferDirty = true;
	bool m_initialized = false;

	static constexpr size_t INITIAL_CAPACITY = 512;
	static constexpr size_t GROWTH_FACTOR = 2;
};