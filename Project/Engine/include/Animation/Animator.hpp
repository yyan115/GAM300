#pragma once
#include <pch.h>
#include <Animation/Animation.hpp>
#include "ECS/Entity.hpp"

class ECSManager;

class ENGINE_API Animator
{
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt, bool isLoop, Entity entity, float speed = 1.0f);

	void PlayAnimation(Animation* pAnimation, Entity entity);
	void ClearAnimation() { mCurrentAnimation = nullptr; mIsBlending = false; mPrevAnimation = nullptr; }
	bool HasAnimation() const { return mCurrentAnimation != nullptr; }

	// Crossfade blending
	void StartCrossfade(Animation* newAnim, float duration, bool prevLoop, Entity entity);
	bool IsBlending() const { return mIsBlending; }

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent = false);
	void CalculateBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
		ECSManager& ecsManager, const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse);

	//const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return mFinalBoneMatrices; }
	float GetCurrentTime() const { return mCurrentTime; }
	void SetCurrentTime(float time, Entity entity); // For editor preview

private:
	void CalculateBlendedBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent, float blendFactor);
	void CalculateBlendedBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
		ECSManager& ecsManager, const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse, float blendFactor);

	Animation* mCurrentAnimation = nullptr;
	float mCurrentTime = 0.0f;

	// Crossfade blending state
	Animation* mPrevAnimation = nullptr;
	float mPrevTime = 0.0f;
	float mBlendDuration = 0.0f;
	float mBlendElapsed = 0.0f;
	bool mIsBlending = false;
	bool mPrevIsLoop = false;
};