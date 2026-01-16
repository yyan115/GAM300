#include "Graphics/Model/ModelFactory.hpp"
#include <ECS/ECSRegistry.hpp>
#include "Transform/Quaternion.hpp"
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/EntityGUIDRegistry.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include <ECS/NameComponent.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include <Asset Manager/AssetManager.hpp>

Entity ModelFactory::SpawnModelHierarchy(const Model& model, const std::string& modelPath, Vector3D position) {
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

	// Create entity for the model root node.
	Entity root = ecs.CreateEntity();
	// Update the existing name component (CreateEntity already adds one)
	if (ecs.TryGetComponent<NameComponent>(root).has_value()) {
		ecs.GetComponent<NameComponent>(root).name = model.modelName;
	}

	// Update Transform component.
	if (ecs.TryGetComponent<Transform>(root).has_value()) {
		Transform& transform = ecs.GetComponent<Transform>(root);
		transform.localPosition = position;
		transform.localRotation = Quaternion{};
		transform.localScale = Vector3D{ 0.1f, 0.1f, 0.1f };
		transform.isDirty = true;
	}

	ModelRenderComponent modelRenderer;
	modelRenderer.model = ResourceManager::GetInstance().GetResource<Model>(modelPath);
	modelRenderer.modelGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(modelPath);
	modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"));
	modelRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(ResourceManager::GetPlatformShaderPath("default"));
	if (modelRenderer.model->meshes[0].material) {
		modelRenderer.material = modelRenderer.model->meshes[0].material;
		std::string materialPath = AssetManager::GetInstance().GetAssetPathFromAssetName(modelRenderer.material->GetName() + ".mat");
		modelRenderer.materialGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(materialPath);
	}
	modelRenderer.boneNameToEntityMap[model.modelName] = root;

	// Check if model has bone info (indicates it's an animated model)
	bool hasAnimations = !modelRenderer.model->mBoneInfoMap.empty();

	SpawnModelNode(model.rootNode, MAX_ENTITIES, modelRenderer.boneNameToEntityMap, root); // Pass hasAnimations flag
	ecs.AddComponent<ModelRenderComponent>(root, modelRenderer);
	return root;
}

// Helper to check for the Assimp FBX tag (same as in your Animation.cpp)
static bool IsAssimpFbxTrsNode(const std::string& n) {
    return n.find("_$AssimpFbx$") != std::string::npos;
}

Entity ModelFactory::SpawnModelNode(const ModelNode& node, Entity parent, 
                                    std::map<std::string, Entity>& boneNameToEntityMap, 
                                    Entity currentEntt,
                                    const Matrix4x4& parentAccumulator) 
{
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // 1. Check if this is a "Dummy" node that should be stripped
    if (IsAssimpFbxTrsNode(node.name))
    {
        // DO NOT CREATE AN ENTITY.
        // Instead, accumulate this node's transform into the accumulator.
        // We multiply the incoming accumulator by this node's local transform.
        Matrix4x4 newAccumulator = parentAccumulator * node.localTransform;

        // Recurse to children immediately, passing the 'parent' (skipping this node)
        // and the 'newAccumulator'.
        for (const auto& childNode : node.children) {
            SpawnModelNode(childNode, parent, boneNameToEntityMap, MAX_ENTITIES, newAccumulator);
        }

        // Return MAX_ENTITIES because we didn't create anything here
        return MAX_ENTITIES;
    }

    // 2. CREATE ENTITY
    std::string finalName = node.name;

    if (currentEntt == MAX_ENTITIES) {
        currentEntt = ecs.CreateEntity();

        if (ecs.TryGetComponent<NameComponent>(currentEntt).has_value()) {
            ecs.GetComponent<NameComponent>(currentEntt).name = finalName;
        }

        if (ecs.TryGetComponent<Transform>(currentEntt).has_value()) {
            Transform& transform = ecs.GetComponent<Transform>(currentEntt);

            // [CRITICAL FIX]
            // Bake the accumulator (from any skipped parents) into THIS node's bind pose.
            // This ensures the 180 rotation is applied here, effectively replacing the deleted parent.
            Matrix4x4 finalLocalTransform = parentAccumulator * node.localTransform;

            transform.localPosition = Matrix4x4::ExtractTranslation(finalLocalTransform);
            transform.localRotation = Quaternion::FromMatrix(finalLocalTransform);
            transform.localScale = Matrix4x4::ExtractScale(finalLocalTransform);
            transform.isDirty = true;
        }
    }

    boneNameToEntityMap[finalName] = currentEntt;

    // 3. REGISTER WITH PARENT (The Lazy Logic)
    if (parent != MAX_ENTITIES)
    {
        // A. Ensure ParentComponent
        if (!ecs.HasComponent<ParentComponent>(currentEntt)) {
            ParentComponent parentComp{};
            parentComp.parent = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(parent);
            ecs.AddComponent<ParentComponent>(currentEntt, parentComp);
        }
        else {
            auto& parentComp = ecs.GetComponent<ParentComponent>(currentEntt);
            parentComp.parent = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(parent);
        }

        // B. Add self to Parent's ChildrenComponent
        // CHECK: Does parent already have the component?
        if (!ecs.HasComponent<ChildrenComponent>(parent)) {
            // NO: Create it now (Lazy Initialization)
            ecs.AddComponent<ChildrenComponent>(parent, ChildrenComponent{});
        }

        // NOW we can safely push back
        auto& parentChildren = ecs.GetComponent<ChildrenComponent>(parent);
        parentChildren.children.push_back(EntityGUIDRegistry::GetInstance().GetGUIDByEntity(currentEntt));
    }

    // 4. RECURSE (Removed the proactive AddComponent here)
    if (!node.children.empty()) {
        for (const auto& childNode : node.children) {
            SpawnModelNode(childNode, currentEntt, boneNameToEntityMap, MAX_ENTITIES, Matrix4x4::Identity());
        }
    }

    return currentEntt;
}

void ModelFactory::PopulateBoneNameToEntityMap(Entity rootEntity, std::map<std::string, Entity>& boneNameToEntityMap)
{
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    if (ecs.HasComponent<ChildrenComponent>(rootEntity)) {
        auto& childComp = ecs.GetComponent<ChildrenComponent>(rootEntity);
        for (auto& childGUID : childComp.children) {
			Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGUID);
			boneNameToEntityMap[ecs.GetComponent<NameComponent>(child).name] = child;
			PopulateBoneNameToEntityMap(child, boneNameToEntityMap);
        }
    }
}