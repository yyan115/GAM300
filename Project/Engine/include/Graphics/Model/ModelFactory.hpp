#pragma once
#include "Graphics/Model/Model.h"
#include <ECS/Entity.hpp>

class ModelFactory {
public:
	static ENGINE_API Entity SpawnModelHierarchy(const Model& model, const std::string& modelPath, Vector3D position);

	static Entity SpawnModelNode(const ModelNode& node, Entity parent,
        std::map<std::string, Entity>& boneNameToEntityMap,
        Entity currentEntt, const Matrix4x4& parentAccumulator = Matrix4x4::Identity());

    static void PopulateBoneNameToEntityMap(Entity rootEntity, std::map<std::string, Entity>& boneNameToEntityMap);
};