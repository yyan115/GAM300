#pragma once
#include <pch.h>
#include <Animation/Animation.hpp>

class Animator
{
public:
	Animator(Animation* animation);

	void UpdateAnimation(float dt);

	void PlayAnimation(Animation* pAnimation);

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

	std::vector<glm::mat4> GetFinalBoneMatrices() { return mFinalBoneMatrices; }

private:
	Animation* mCurrentAnimation;
	float mCurrentTime;
	float mDeltaTime;
	std::vector<glm::mat4> mFinalBoneMatrices;
};