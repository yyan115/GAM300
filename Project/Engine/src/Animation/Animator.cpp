#include <pch.h>
#include "Animation/Animator.hpp"

Animator::Animator(Animation* animation)
{
	mCurrentTime = 0.0f;
	mCurrentAnimation = animation;
	mFinalBoneMatrices.assign(100, glm::mat4(1.0f));
}

void Animator::UpdateAnimation(float dt, bool isLoop)
{
	if (!mCurrentAnimation) return; // No animation to play
	
	float tps = mCurrentAnimation->GetTicksPerSecond();
	if (tps <= 0.0f) tps = 25.0f; // Default to 25 if invalid

	mCurrentTime += tps * dt;
	float duration = mCurrentAnimation->GetDuration();
	if (isLoop)
	{
		mCurrentTime = fmod(mCurrentTime, duration);
	}
	else
	{
		if (mCurrentTime > duration)
			mCurrentTime = duration;
	}

	CalculateBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f));
}

void Animator::PlayAnimation(Animation* pAnimation)
{
	mCurrentAnimation = pAnimation;
	mCurrentTime = 0.0f;
	if(pAnimation)
		mFinalBoneMatrices.assign(pAnimation->GetBoneIDMap().size(), glm::mat4(1.0f));
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform)
{
	std::string nodeName = node->name;
	glm::mat4 nodeTransform = node->transformation;

	Bone* Bone = mCurrentAnimation->FindBone(nodeName);

	if (Bone)
	{
		Bone->Update(mCurrentTime);
		nodeTransform = Bone->GetLocalTransform();
	}
	glm::mat4 globalTransformation = parentTransform * nodeTransform;
	auto boneInfoMap = mCurrentAnimation->GetBoneIDMap();
	if (boneInfoMap.find(nodeName) != boneInfoMap.end())
	{
		int index = boneInfoMap[nodeName].id;
		glm::mat4 offset = boneInfoMap[nodeName].offset;
		mFinalBoneMatrices[index] = globalTransformation * offset;
	}
	for (int i = 0; i < node->childrenCount; i++)
	{
		CalculateBoneTransform(&node->children[i], globalTransformation);
	}
}