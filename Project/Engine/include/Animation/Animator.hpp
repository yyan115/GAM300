#pragma once
#include <pch.h>
#include <Animation/Animation.hpp>

class Animator
{
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt, bool isLoop);

	void PlayAnimation(Animation* pAnimation);

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

	const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return mFinalBoneMatrices; }
	float GetCurrentTime() const { return mCurrentTime; }

private:
	Animation* mCurrentAnimation;
	float mCurrentTime;
	std::vector<glm::mat4> mFinalBoneMatrices;
};