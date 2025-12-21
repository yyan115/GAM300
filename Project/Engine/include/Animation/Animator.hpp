#pragma once
#include <pch.h>
#include <Animation/Animation.hpp>

class ENGINE_API Animator
{
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt, bool isLoop);

	void PlayAnimation(Animation* pAnimation);
	void ClearAnimation() { mCurrentAnimation = nullptr; }
	bool HasAnimation() const { return mCurrentAnimation != nullptr; }

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

	const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return mFinalBoneMatrices; }
	float GetCurrentTime() const { return mCurrentTime; }
	void SetCurrentTime(float time); // For editor preview

private:
	Animation* mCurrentAnimation = nullptr;
	float mCurrentTime = 0.0f;
	std::vector<glm::mat4> mFinalBoneMatrices;
};