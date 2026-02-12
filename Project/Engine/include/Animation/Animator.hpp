#pragma once
#include <pch.h>
#include <Animation/Animation.hpp>
#include "ECS/Entity.hpp"

class ECSManager;

class ENGINE_API Animator
{
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt, bool isLoop, Entity entity);

	void PlayAnimation(Animation* pAnimation, Entity entity);
	void ClearAnimation() { mCurrentAnimation = nullptr; }
	bool HasAnimation() const { return mCurrentAnimation != nullptr; }

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent = false);
	void CalculateBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
		ECSManager& ecsManager, const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse);

	//const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return mFinalBoneMatrices; }
	float GetCurrentTime() const { return mCurrentTime; }
	void SetCurrentTime(float time, Entity entity); // For editor preview

private:
	Animation* mCurrentAnimation = nullptr;
	float mCurrentTime = 0.0f;
};