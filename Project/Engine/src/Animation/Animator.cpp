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
	//int n = std::min<int>(3, mFinalBoneMatrices.size());
	//for (int i = 0; i < n; ++i)
	//{
	//	ENGINE_LOG_DEBUG("[FinalBoneMatrix] Index " + std::to_string(i) + ":\n");
	//	const glm::mat4& m = mFinalBoneMatrices[i];
	//	for (int row = 0; row < 4; ++row)
	//	{
	//		for (int col = 0; col < 4; ++col)
	//		{
	//			// glm is column-major: m[col][row]
	//			ENGINE_LOG_DEBUG(std::to_string(m[col][row]) + " ");
	//		}
	//	}
	//}
}

void Animator::PlayAnimation(Animation* pAnimation)
{
	mCurrentAnimation = pAnimation;
	mCurrentTime = 0.0f;
	if (pAnimation) 
	{
		size_t n = pAnimation->GetBoneIDMap().size();
		mFinalBoneMatrices.assign(n ? n : 1, glm::mat4(1.0f));
	}

}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform)
{
    std::string nodeName = node->name;
    glm::mat4 nodeTransform = node->transformation;

    Bone* Bone = mCurrentAnimation->FindBone(nodeName);

    //bool isKeyBone = (nodeName == "mixamorig:Hips" || nodeName == "mixamorig:Spine");

    if (Bone)
    {
        Bone->Update(mCurrentTime);
        nodeTransform = Bone->GetLocalTransform();

        //if (isKeyBone) {
        //    ENGINE_LOG_DEBUG("[CalcBone] '" + nodeName + "' ANIMATED at time " + std::to_string(mCurrentTime) + "\n");
        //    ENGINE_LOG_DEBUG("  LocalTransform[3]: [" +
        //        std::to_string(nodeTransform[3][0]) + ", " +
        //        std::to_string(nodeTransform[3][1]) + ", " +
        //        std::to_string(nodeTransform[3][2]) + "]\n");
        //}
    }
    //else {
    //    if (isKeyBone) {
    //        ENGINE_LOG_DEBUG("[CalcBone] '" + nodeName + "' NOT ANIMATED (using hierarchy transform)\n");
    //        ENGINE_LOG_DEBUG("  HierarchyTransform[3]: [" +
    //            std::to_string(nodeTransform[3][0]) + ", " +
    //            std::to_string(nodeTransform[3][1]) + ", " +
    //            std::to_string(nodeTransform[3][2]) + "]\n");
    //    }
    //}

    glm::mat4 globalTransformation = parentTransform * nodeTransform;
    auto boneInfoMap = mCurrentAnimation->GetBoneIDMap();

    if (boneInfoMap.find(nodeName) != boneInfoMap.end())
    {
        int index = boneInfoMap[nodeName].id;
        glm::mat4 offset = boneInfoMap[nodeName].offset;
        glm::mat4 globalInverse = mCurrentAnimation->GetGlobalInverse();

        //if (isKeyBone) {
        //    ENGINE_LOG_DEBUG("[CalcBone] '" + nodeName + "' matrix calculation:\n");
        //    ENGINE_LOG_DEBUG("  ParentTransform[3]: [" +
        //        std::to_string(parentTransform[3][0]) + ", " +
        //        std::to_string(parentTransform[3][1]) + ", " +
        //        std::to_string(parentTransform[3][2]) + "]\n");
        //    ENGINE_LOG_DEBUG("  GlobalTransform[3]: [" +
        //        std::to_string(globalTransformation[3][0]) + ", " +
        //        std::to_string(globalTransformation[3][1]) + ", " +
        //        std::to_string(globalTransformation[3][2]) + "]\n");
        //    ENGINE_LOG_DEBUG("  Offset[3]: [" +
        //        std::to_string(offset[3][0]) + ", " +
        //        std::to_string(offset[3][1]) + ", " +
        //        std::to_string(offset[3][2]) + "]\n");
        //    ENGINE_LOG_DEBUG("  GlobalInverse[3]: [" +
        //        std::to_string(globalInverse[3][0]) + ", " +
        //        std::to_string(globalInverse[3][1]) + ", " +
        //        std::to_string(globalInverse[3][2]) + "]\n");
        //}

        mFinalBoneMatrices[index] = globalInverse * globalTransformation * offset;

        //if (isKeyBone) {
        //    ENGINE_LOG_DEBUG("  FinalMatrix[3]: [" +
        //        std::to_string(mFinalBoneMatrices[index][3][0]) + ", " +
        //        std::to_string(mFinalBoneMatrices[index][3][1]) + ", " +
        //        std::to_string(mFinalBoneMatrices[index][3][2]) + "]\n");
        //}
    }

    for (int i = 0; i < node->childrenCount; i++)
    {
        CalculateBoneTransform(&node->children[i], globalTransformation);
    }
}