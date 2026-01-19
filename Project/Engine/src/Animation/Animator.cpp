#include <pch.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include "Animation/Animator.hpp"
#include <ECS/ECSRegistry.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/EntityGUIDRegistry.hpp>
#include <ECS/NameComponent.hpp>

Animator::Animator(Animation* animation)
{
	mCurrentTime = 0.0f;
	mCurrentAnimation = animation;
}

void Animator::UpdateAnimation(float dt, bool isLoop, Entity entity)
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

	CalculateBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f), entity);
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

void Animator::PlayAnimation(Animation* pAnimation, Entity entity)
{
	mCurrentAnimation = pAnimation;
	mCurrentTime = 0.0f;
	if (pAnimation)
	{
		size_t n = pAnimation->GetBoneIDMap().size();
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        if (ecsManager.HasComponent<ModelRenderComponent>(entity)) {
		    ecsManager.GetComponent<ModelRenderComponent>(entity).mFinalBoneMatrices.assign(n ? n : 1, glm::mat4(1.0f));
		    CalculateBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f), entity);
        }
	}
}

void Animator::SetCurrentTime(float time, Entity entity)
{
	mCurrentTime = time;
	// Update bone transforms for the new time
	if (mCurrentAnimation) {
		CalculateBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f), entity);
	}
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent)
{
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    bool isRoot = (node == &mCurrentAnimation->GetRootNode());
    
    std::string nodeName{};
    if (isRoot) {
        nodeName = ecsManager.GetComponent<NameComponent>(entity).name;
    }
    else {
        nodeName = node->name;
    }

    glm::mat4 nodeTransform = node->transformation; // Default Bind Pose

    // 1. Calculate Animation Matrix
    Bone* bone = mCurrentAnimation->FindBone(nodeName);
    if (bone)
    {
        bone->Update(mCurrentTime);
        nodeTransform = bone->GetLocalTransform();
    }

    // 3. Update ECS Entity
    auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
    if (modelComp.boneNameToEntityMap.find(nodeName) != modelComp.boneNameToEntityMap.end())
    {
        Entity boneEntity = modelComp.boneNameToEntityMap[nodeName];
        if (boneEntity != MAX_ENTITIES && ecsManager.HasComponent<Transform>(boneEntity))
        {
            if (!isRoot)
            {
                // [CHILD STRATEGY]
                glm::mat4 matrixToApply = nodeTransform;

                // CRITICAL FIX: If the parent was the Root (and we forced it to identity),
                // we must "Bake" the parent's transform into this child so the motion isn't lost.
                if (bakeParent)
                {
                    // parentTransform here IS the Root's animation transform
                    matrixToApply = parentTransform * nodeTransform;
                }

                // Decompose and Apply
                glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
                glm::decompose(matrixToApply, scale, rotation, translation, skew, perspective);

                ecsManager.transformSystem->SetLocalPosition(boneEntity, Vector3D::ConvertGLMToVector3D(translation));

                // Use (w, x, y, z) matching your struct
                Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
                ecsManager.transformSystem->SetLocalRotation(boneEntity, engineRot);

                ecsManager.transformSystem->SetLocalScale(boneEntity, Vector3D::ConvertGLMToVector3D(scale));
            }
        }
    }

    // 4. Calculate Global Transform for Recursion & Shader
    glm::mat4 globalTransformation = parentTransform * nodeTransform;

    // 5. Update Shader Matrices (Standard Logic - Unchanged)
    auto boneInfoMap = mCurrentAnimation->GetBoneIDMap();
    if (boneInfoMap.find(nodeName) != boneInfoMap.end())
    {
        int index = boneInfoMap[nodeName].id;
        glm::mat4 offset = boneInfoMap[nodeName].offset;
        glm::mat4 globalInverse = mCurrentAnimation->GetGlobalInverse();
        ecsManager.GetComponent<ModelRenderComponent>(entity).mFinalBoneMatrices[index] =
            globalInverse * globalTransformation * offset;
    }

    // 6. Recurse
    for (int i = 0; i < node->childrenCount; i++)
    {
        // If WE are the root, tell our children to bake our transform.
        // If we are NOT the root, pass 'false' (children behave normally).
        bool shouldChildBake = isRoot;

        CalculateBoneTransform(&node->children[i], globalTransformation, entity, shouldChildBake);
    }
}