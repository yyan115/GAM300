#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "InstanceBatch.hpp"
#include "ECS/ECSManager.hpp"

class Model;
class Material;
class Shader;
class ModelRenderComponent;
class Frustum;

struct BatchKey {
	Model* model;
	Material* material;
	Shader* shader;

	bool operator==(const BatchKey& other) const {
		return model == other.model &&
			material == other.material &&
			shader == other.shader;
	}
};

struct BatchKeyHash {
	size_t operator()(const BatchKey& key) const {
		size_t h1 = std::hash<void*>()(key.model);
		size_t h2 = std::hash<void*>()(key.material);
		size_t h3 = std::hash<void*>()(key.shader);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

struct InstancingStats {
	int totalObjects = 0;
	int instancedObjects = 0;
	int nonInstancedObjects = 0;
	int batchCount = 0;
	int drawCalls = 0;
	int culledObjects = 0;

	void Reset() {
		totalObjects = 0;
		instancedObjects = 0;
		nonInstancedObjects = 0;
		batchCount = 0;
		drawCalls = 0;
		culledObjects = 0;
	}

	float GetBatchEfficiency() const
	{
		if (instancedObjects == 0) return 0.0f;
		return 100.0f * (1.0f - (float)batchCount / (float)instancedObjects);
	} 
};

class ENGINE_API InstancingManager {
public:
	static InstancingManager& GetInstance();

	void SetEnabled(bool enabled) { m_enabled = enabled; }
	bool IsEnabled() const { return m_enabled; }

	void SetMinInstancesForBatching(int count) { m_minInstancesForBatching = count; }
	int GetMinInstancesForBatching() const { return m_minInstancesForBatching; }

	void BeginFrame();
	void EndFrame();

	bool TryAddInstance(const ModelRenderComponent& component, const glm::mat4& worldMatrix, const glm::vec3& bloomColor = glm::vec3(0.0f), float bloomIntensity = 0.0f);

	void RenderBatches(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);

	void RenderBatchesDepthOnly(const glm::mat4& lightSpaceMatrix);

	// Camera-space depth prepass — renders all opaque instanced batches into the depth buffer
	// using the provided prepass shader (depth write only, no colour output).
	void RenderBatchesDepthPrepass(const glm::mat4& view, const glm::mat4& projection, Shader& depthShader);

	void SetFrustum(const Frustum* frustum) { m_frustum = frustum; }

	const InstancingStats& GetStats() const { return m_stats; }

	void PrewarmScene(ECSManager& ecsManager);

	bool WasRenderedInstanced(const ModelRenderComponent& component) const;

private:
	InstancingManager() = default;
	~InstancingManager() = default;

	InstancingManager(const InstancingManager&) = delete;
	InstancingManager& operator=(const InstancingManager&) = delete;

	bool IsInstanceable(const ModelRenderComponent& component) const;
	InstanceBatch& GetOrCreateBatch(const BatchKey& key, std::shared_ptr<Model> model, std::shared_ptr<Material> material, std::shared_ptr<Shader> shader);
	bool m_enabled = true;

	int m_minInstancesForBatching = 2;
	std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash> m_batches;
	std::vector<InstanceBatch*> m_sortedBatches;
	const Frustum* m_frustum = nullptr;
	InstancingStats m_stats;
};