#include <pch.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
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

void Animator::UpdateAnimation(float dt, bool isLoop, Entity entity, float speed)
{
	if (!mCurrentAnimation) return; // No animation to play

	float animDt = dt * speed; // Speed-scaled dt for animation time advancement

	if (mIsBlending)
	{
		// Use RAW dt for blend timing so crossfade duration is independent of animation speed
		mBlendElapsed += dt;

		// Advance current (new) animation time with speed scaling
		float tps = mCurrentAnimation->GetTicksPerSecond();
		if (tps <= 0.0f) tps = 25.0f;
		mCurrentTime += tps * animDt;

		float duration = mCurrentAnimation->GetDuration();
		if (isLoop)
			mCurrentTime = fmod(mCurrentTime, duration);
		else if (mCurrentTime > duration)
			mCurrentTime = duration;

		// Advance previous animation time with speed scaling
		if (mPrevAnimation)
		{
			float prevTps = mPrevAnimation->GetTicksPerSecond();
			if (prevTps <= 0.0f) prevTps = 25.0f;
			mPrevTime += prevTps * animDt;

			float prevDuration = mPrevAnimation->GetDuration();
			if (mPrevIsLoop)
				mPrevTime = fmod(mPrevTime, prevDuration);
			else if (mPrevTime > prevDuration)
				mPrevTime = prevDuration;
		}

		float blendFactor = std::clamp(mBlendElapsed / mBlendDuration, 0.0f, 1.0f);

		if (blendFactor >= 1.0f)
		{
			// Blend complete - switch fully to current animation
			mIsBlending = false;
			mPrevAnimation = nullptr;
			CalculateBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f), entity);
		}
		else
		{
			CalculateBlendedBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f), entity, false, blendFactor);
		}
	}
	else
	{
		// Normal (non-blending) path
		float tps = mCurrentAnimation->GetTicksPerSecond();
		if (tps <= 0.0f) tps = 25.0f;

		mCurrentTime += tps * animDt;

		float duration = mCurrentAnimation->GetDuration();
		if (isLoop)
			mCurrentTime = fmod(mCurrentTime, duration);
		else if (mCurrentTime > duration)
			mCurrentTime = duration;

		CalculateBoneTransform(&mCurrentAnimation->GetRootNode(), glm::mat4(1.0f), entity);
	}
}

void Animator::PlayAnimation(Animation* pAnimation, Entity entity)
{
	mCurrentAnimation = pAnimation;
	mCurrentTime = 0.0f;
	mIsBlending = false;
	mPrevAnimation = nullptr;
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

void Animator::StartCrossfade(Animation* newAnim, float duration, bool prevLoop, Entity entity)
{
	if (!newAnim || duration <= 0.0f)
	{
		PlayAnimation(newAnim, entity);
		return;
	}

	// Store current animation as previous
	mPrevAnimation = mCurrentAnimation;
	mPrevTime = mCurrentTime;
	mPrevIsLoop = prevLoop;

	// Set new animation as current
	mCurrentAnimation = newAnim;
	mCurrentTime = 0.0f;

	// Set blend state
	mBlendDuration = duration;
	mBlendElapsed = 0.0f;
	mIsBlending = true;

	// Ensure bone matrices are allocated for the larger skeleton
	if (newAnim)
	{
		size_t n = newAnim->GetBoneIDMap().size();
		if (mPrevAnimation)
			n = std::max(n, mPrevAnimation->GetBoneIDMap().size());

		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (ecsManager.HasComponent<ModelRenderComponent>(entity)) {
			auto& matrices = ecsManager.GetComponent<ModelRenderComponent>(entity).mFinalBoneMatrices;
			if (matrices.size() < (n ? n : 1))
				matrices.resize(n ? n : 1, glm::mat4(1.0f));
		}
	}
}

void Animator::CalculateBlendedBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent, float blendFactor)
{
	CalculateBlendedBoneTransformInternal(node, parentTransform, entity, bakeParent,
		ECSRegistry::GetInstance().GetActiveECSManager(),
		mCurrentAnimation->GetBoneIDMap(),
		mCurrentAnimation->GetGlobalInverse(),
		blendFactor);
}

void Animator::CalculateBlendedBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
	ECSManager& ecsManager, const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse, float blendFactor)
{
	bool isRoot = (node == &mCurrentAnimation->GetRootNode());

	const std::string& nodeName = isRoot
		? ecsManager.GetComponent<NameComponent>(entity).name
		: node->name;

	glm::mat4 nodeTransform = node->transformation; // Default bind pose

	// 1. Hoist these variables so the ECS update can use them!
	glm::vec3 finalPos;
	glm::quat finalRot;
	glm::vec3 finalScale;
	bool hasBlendedTRS = false; // Track if we actually calculated a blend

	// Look up bone in both animations
	Bone* oldBone = mPrevAnimation ? mPrevAnimation->FindBone(nodeName) : nullptr;
	Bone* newBone = mCurrentAnimation->FindBone(nodeName);

	if (oldBone && newBone)
	{
		oldBone->Update(mPrevTime);
		newBone->Update(mCurrentTime);

		finalPos = glm::mix(oldBone->GetLocalPosition(), newBone->GetLocalPosition(), blendFactor);
		finalRot = glm::slerp(oldBone->GetLocalRotation(), newBone->GetLocalRotation(), blendFactor);
		finalScale = glm::mix(oldBone->GetLocalScale(), newBone->GetLocalScale(), blendFactor);
		hasBlendedTRS = true;

		nodeTransform = glm::translate(glm::mat4(1.0f), finalPos)
			* glm::mat4_cast(finalRot)
			* glm::scale(glm::mat4(1.0f), finalScale);
	}
	else if (oldBone || newBone)
	{
		glm::vec3 bindPos, bindScale, skew;
		glm::quat bindRot;
		glm::vec4 perspective;
		glm::decompose(node->transformation, bindScale, bindRot, bindPos, skew, perspective);

		glm::vec3 srcPos, srcScale, dstPos, dstScale;
		glm::quat srcRot, dstRot;

		if (oldBone)
		{
			oldBone->Update(mPrevTime);
			srcPos = oldBone->GetLocalPosition(); srcRot = oldBone->GetLocalRotation(); srcScale = oldBone->GetLocalScale();
			dstPos = bindPos; dstRot = bindRot; dstScale = bindScale;
		}
		else
		{
			newBone->Update(mCurrentTime);
			srcPos = bindPos; srcRot = bindRot; srcScale = bindScale;
			dstPos = newBone->GetLocalPosition(); dstRot = newBone->GetLocalRotation(); dstScale = newBone->GetLocalScale();
		}

		finalPos = glm::mix(srcPos, dstPos, blendFactor);
		finalRot = glm::slerp(srcRot, dstRot, blendFactor);
		finalScale = glm::mix(srcScale, dstScale, blendFactor);
		hasBlendedTRS = true;

		nodeTransform = glm::translate(glm::mat4(1.0f), finalPos)
			* glm::mat4_cast(finalRot)
			* glm::scale(glm::mat4(1.0f), finalScale);
	}

	// 2. ECS Update (The Optimized Part)
	auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
	auto boneIt = modelComp.boneNameToEntityMap.find(nodeName);
	if (boneIt != modelComp.boneNameToEntityMap.end())
	{
		Entity boneEntity = boneIt->second;
		if (boneEntity != INVALID_ENTITY && ecsManager.HasComponent<Transform>(boneEntity))
		{
			if (!isRoot)
			{
				if (bakeParent)
				{
					// SLOW PATH: We must multiply and decompose
					glm::mat4 matrixToApply = parentTransform * nodeTransform;
					glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
					glm::decompose(matrixToApply, scale, rotation, translation, skew, perspective);

					Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
					ecsManager.transformSystem->SetLocalTransform(boneEntity, Vector3D::ConvertGLMToVector3D(translation), engineRot, Vector3D::ConvertGLMToVector3D(scale));
				}
				else
				{
					// FAST PATH
					if (hasBlendedTRS)
					{
						// We already have the math! Just plug it straight into the engine.
						Quaternion engineRot(finalRot.w, finalRot.x, finalRot.y, finalRot.z);
						ecsManager.transformSystem->SetLocalTransform(
							boneEntity,
							Vector3D::ConvertGLMToVector3D(finalPos),
							engineRot,
							Vector3D::ConvertGLMToVector3D(finalScale)
						);
					}
					else
					{
						// Fallback: This is a static bind-pose bone (not animated in either clip)
						glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
						glm::decompose(node->transformation, scale, rotation, translation, skew, perspective);

						Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
						ecsManager.transformSystem->SetLocalTransform(boneEntity, Vector3D::ConvertGLMToVector3D(translation), engineRot, Vector3D::ConvertGLMToVector3D(scale));
					}
				}
			}
		}
	}

	// 3. Calculate global transform for recursion and shader
	glm::mat4 globalTransformation = parentTransform * nodeTransform;

	// 4. Update shader matrices
	auto infoIt = boneInfoMap.find(nodeName);
	if (infoIt != boneInfoMap.end())
	{
		int index = infoIt->second.id;
		const glm::mat4& offset = infoIt->second.offset;
		modelComp.mFinalBoneMatrices[index] = globalInverse * globalTransformation * offset;
	}

	// 5. Recurse into children
	for (int i = 0; i < node->childrenCount; i++)
	{
		bool shouldChildBake = isRoot;
		CalculateBlendedBoneTransformInternal(&node->children[i], globalTransformation, entity, shouldChildBake,
			ecsManager, boneInfoMap, globalInverse, blendFactor);
	}
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent)
{
    CalculateBoneTransformInternal(node, parentTransform, entity, bakeParent,
        ECSRegistry::GetInstance().GetActiveECSManager(),
        mCurrentAnimation->GetBoneIDMap(),
        mCurrentAnimation->GetGlobalInverse());
}

void Animator::CalculateBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
    ECSManager& ecsManager, const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse)
{
    bool isRoot = (node == &mCurrentAnimation->GetRootNode());

    const std::string& nodeName = isRoot
        ? ecsManager.GetComponent<NameComponent>(entity).name
        : node->name;

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
    auto boneIt = modelComp.boneNameToEntityMap.find(nodeName);
    if (boneIt != modelComp.boneNameToEntityMap.end())
    {
        Entity boneEntity = boneIt->second;
        if (boneEntity != INVALID_ENTITY && ecsManager.HasComponent<Transform>(boneEntity))
        {
            if (!isRoot)
            {
				if (bakeParent)
				{
					// SLOW PATH: We must multiply and decompose
					glm::mat4 matrixToApply = parentTransform * nodeTransform;
					glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
					glm::decompose(matrixToApply, scale, rotation, translation, skew, perspective);

					Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
					ecsManager.transformSystem->SetLocalTransform(boneEntity, Vector3D::ConvertGLMToVector3D(translation), engineRot, Vector3D::ConvertGLMToVector3D(scale));
				}
				else
				{
					// FAST PATH: Skip matrix math completely! Get TRS directly from the Bone.
					if (bone)
					{
						// The bone was animated this frame. Use its direct values.
						Quaternion engineRot(bone->GetLocalRotation().w, bone->GetLocalRotation().x, bone->GetLocalRotation().y, bone->GetLocalRotation().z);

						ecsManager.transformSystem->SetLocalTransform(
							boneEntity,
							Vector3D::ConvertGLMToVector3D(bone->GetLocalPosition()),
							engineRot,
							Vector3D::ConvertGLMToVector3D(bone->GetLocalScale())
						);
					}
					else
					{
						// Fallback: This is a static bind-pose bone (no animation). 
						// We only decompose it once (or you can cache this in AssimpNodeData!)
						glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
						glm::decompose(node->transformation, scale, rotation, translation, skew, perspective);
						Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
						ecsManager.transformSystem->SetLocalTransform(boneEntity, Vector3D::ConvertGLMToVector3D(translation), engineRot, Vector3D::ConvertGLMToVector3D(scale));
					}
				}
            }
            //else 
            //{
            //    glm::mat4 matrixToApply = nodeTransform;

            //    // Decompose and Apply
            //    glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
            //    glm::decompose(matrixToApply, scale, rotation, translation, skew, perspective);

            //    ecsManager.transformSystem->SetLocalPosition(boneEntity, Vector3D::ConvertGLMToVector3D(translation));

            //    // Use (w, x, y, z) matching your struct
            //    Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
            //    ecsManager.transformSystem->SetLocalRotation(boneEntity, engineRot);

            //    ecsManager.transformSystem->SetLocalScale(boneEntity, Vector3D::ConvertGLMToVector3D(scale));
            //}
        }
    }

    // 4. Calculate Global Transform for Recursion & Shader
    glm::mat4 globalTransformation = parentTransform * nodeTransform;

    // 5. Update Shader Matrices
    auto infoIt = boneInfoMap.find(nodeName);
    if (infoIt != boneInfoMap.end())
    {
        int index = infoIt->second.id;
        const glm::mat4& offset = infoIt->second.offset;
        modelComp.mFinalBoneMatrices[index] =
            globalInverse * globalTransformation * offset;
    }

    // 6. Recurse
    for (int i = 0; i < node->childrenCount; i++)
    {
        bool shouldChildBake = isRoot;
        CalculateBoneTransformInternal(&node->children[i], globalTransformation, entity, shouldChildBake,
            ecsManager, boneInfoMap, globalInverse);
    }
}