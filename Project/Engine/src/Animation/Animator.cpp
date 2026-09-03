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

namespace {
glm::mat4 MultiplyAffine(
	const glm::mat4& lhs,
	const glm::mat4& rhs)
{
	const glm::vec3 lhsX(lhs[0]);
	const glm::vec3 lhsY(lhs[1]);
	const glm::vec3 lhsZ(lhs[2]);
	const glm::vec3 rhsTranslation(rhs[3]);

	glm::mat4 result(1.0f);
	result[0] = glm::vec4(
		lhsX * rhs[0].x + lhsY * rhs[0].y + lhsZ * rhs[0].z,
		0.0f);
	result[1] = glm::vec4(
		lhsX * rhs[1].x + lhsY * rhs[1].y + lhsZ * rhs[1].z,
		0.0f);
	result[2] = glm::vec4(
		lhsX * rhs[2].x + lhsY * rhs[2].y + lhsZ * rhs[2].z,
		0.0f);
	result[3] = glm::vec4(
		lhsX * rhsTranslation.x +
		lhsY * rhsTranslation.y +
		lhsZ * rhsTranslation.z + glm::vec3(lhs[3]),
		1.0f);
	return result;
}

glm::mat4 ComposeTRS(
	const glm::vec3& translation,
	const glm::quat& rotation,
	const glm::vec3& scale)
{
	glm::mat4 result = glm::mat4_cast(rotation);
	result[0] *= scale.x;
	result[1] *= scale.y;
	result[2] *= scale.z;
	result[3] = glm::vec4(translation, 1.0f);
	return result;
}
}

Animator::Animator(Animation* animation)
{
	mCurrentTime = 0.0f;
	mCurrentAnimation = animation;
}

void Animator::ResetBoneEntityCache(Entity owner)
{
	mBoneEntityCache.clear();
	mBoneEntityCacheCursor = 0;
	mBoneEntityCacheOwner = owner;
	mTransformSetVersion = 0;
}

void Animator::ResetPreviousBoneCache()
{
	mPreviousBoneCache.clear();
	mPreviousBoneCacheCursor = 0;
	mPreviousBoneCacheAnimation = nullptr;
	mPreviousBoneCacheRevision = 0;
}

Bone* Animator::ResolvePreviousBone(
	const AssimpNodeData* node,
	const std::string& nodeName)
{
	if (!mPrevAnimation) {
		return nullptr;
	}

	const std::uint64_t revision = mPrevAnimation->GetRevision();
	if (mPreviousBoneCacheAnimation != mPrevAnimation ||
		mPreviousBoneCacheRevision != revision) {
		mPreviousBoneCache.clear();
		mPreviousBoneCacheCursor = 0;
		mPreviousBoneCacheAnimation = mPrevAnimation;
		mPreviousBoneCacheRevision = revision;
	}

	const std::size_t cacheIndex = mPreviousBoneCacheCursor++;
	if (cacheIndex < mPreviousBoneCache.size() &&
		mPreviousBoneCache[cacheIndex].node == node) {
		return mPreviousBoneCache[cacheIndex].bone;
	}

	Bone* bone = mPrevAnimation->FindBone(nodeName);
	const PreviousBoneCacheEntry entry{node, bone};
	if (cacheIndex < mPreviousBoneCache.size()) {
		mPreviousBoneCache[cacheIndex] = entry;
	}
	else {
		mPreviousBoneCache.push_back(entry);
	}
	return bone;
}

Transform* Animator::ResolveBoneTransform(
	const AssimpNodeData* node,
	const std::string& nodeName,
	Entity owner,
	const ModelRenderComponent& modelComp,
	ECSManager& ecsManager)
{
	if (mBoneEntityCacheOwner != owner) {
		ResetBoneEntityCache(owner);
	}

	const std::size_t cacheIndex = mBoneEntityCacheCursor++;
	if (cacheIndex < mBoneEntityCache.size() &&
		mBoneEntityCache[cacheIndex].node == node) {
		return mBoneEntityCache[cacheIndex].transform;
	}

	const auto mapped = modelComp.boneNameToEntityMap.find(nodeName);
	Transform* boneTransform = nullptr;
	if (mapped != modelComp.boneNameToEntityMap.end()) {
		if (auto transform = ecsManager.TryGetComponent<Transform>(mapped->second)) {
			boneTransform = &transform->get();
		}
	}

	const BoneTransformCacheEntry entry{node, boneTransform};
	if (cacheIndex < mBoneEntityCache.size()) {
		mBoneEntityCache[cacheIndex] = entry;
	} else {
		mBoneEntityCache.push_back(entry);
	}
	return boneTransform;
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
			ResetPreviousBoneCache();
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
	if (mCurrentAnimation != pAnimation || mBoneEntityCacheOwner != entity) {
		ResetBoneEntityCache(entity);
	}
	mCurrentAnimation = pAnimation;
	mCurrentTime = 0.0f;
	mIsBlending = false;
	mPrevAnimation = nullptr;
	ResetPreviousBoneCache();
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

	if (mCurrentAnimation != newAnim || mBoneEntityCacheOwner != entity) {
		ResetBoneEntityCache(entity);
	}

	// Store current animation as previous
	mPrevAnimation = mCurrentAnimation;
	mPrevTime = mCurrentTime;
	mPrevIsLoop = prevLoop;
	ResetPreviousBoneCache();

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
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	const std::uint64_t transformSetVersion =
		ecsManager.transformSystem->entities.Version();
	if (mBoneEntityCacheOwner != entity ||
		mTransformSetVersion != transformSetVersion) {
		ResetBoneEntityCache(entity);
		mTransformSetVersion = transformSetVersion;
	}
	mBoneEntityCacheCursor = 0;
	mPreviousBoneCacheCursor = 0;

	auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
	const std::string& rootName = ecsManager.GetComponent<NameComponent>(entity).name;
	CalculateBlendedBoneTransformInternal(node, parentTransform, entity, bakeParent, nullptr,
		ecsManager,
		modelComp,
		rootName,
		mCurrentAnimation->GetBoneIDMap(),
		mCurrentAnimation->GetGlobalInverse(),
		blendFactor);
	++modelComp.bonePoseRevision;
}

void Animator::CalculateBlendedBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
	const glm::mat4* bakedParentTransform,
	ECSManager& ecsManager, ModelRenderComponent& modelComp, const std::string& rootName,
	const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse, float blendFactor)
{
	bool isRoot = (node == &mCurrentAnimation->GetRootNode());

	const std::string& nodeName = isRoot
		? rootName
		: node->name;

	glm::mat4 nodeTransform = node->transformation; // Default bind pose

	// 1. Hoist these variables so the ECS update can use them!
	glm::vec3 finalPos;
	glm::quat finalRot;
	glm::vec3 finalScale;
	bool hasBlendedTRS = false; // Track if we actually calculated a blend

	// Look up bone in both animations
	Bone* oldBone = ResolvePreviousBone(node, nodeName);
	Bone* newBone = isRoot ? mCurrentAnimation->FindBone(nodeName) : node->animationBone;

	if (oldBone && newBone)
	{
		oldBone->Update(mPrevTime);
		newBone->Update(mCurrentTime);

		finalPos = glm::mix(oldBone->GetLocalPosition(), newBone->GetLocalPosition(), blendFactor);
		finalRot = glm::slerp(oldBone->GetLocalRotation(), newBone->GetLocalRotation(), blendFactor);
		finalScale = glm::mix(oldBone->GetLocalScale(), newBone->GetLocalScale(), blendFactor);
		hasBlendedTRS = true;

		nodeTransform = ComposeTRS(finalPos, finalRot, finalScale);
	}
	else if (oldBone || newBone)
	{
		glm::vec3 srcPos, srcScale, dstPos, dstScale;
		glm::quat srcRot, dstRot;

		if (oldBone)
		{
			oldBone->Update(mPrevTime);
			srcPos = oldBone->GetLocalPosition(); srcRot = oldBone->GetLocalRotation(); srcScale = oldBone->GetLocalScale();
			dstPos = node->bindTranslation; dstRot = node->bindRotation; dstScale = node->bindScale;
		}
		else
		{
			newBone->Update(mCurrentTime);
			srcPos = node->bindTranslation; srcRot = node->bindRotation; srcScale = node->bindScale;
			dstPos = newBone->GetLocalPosition(); dstRot = newBone->GetLocalRotation(); dstScale = newBone->GetLocalScale();
		}

		finalPos = glm::mix(srcPos, dstPos, blendFactor);
		finalRot = glm::slerp(srcRot, dstRot, blendFactor);
		finalScale = glm::mix(srcScale, dstScale, blendFactor);
		hasBlendedTRS = true;

		nodeTransform = ComposeTRS(finalPos, finalRot, finalScale);
	}

	// The root entity owns the model transform and must not receive a bone-local
	// pose. Cache stable component pointers for all actual bone entities.
	if (!isRoot)
	{
		Transform* boneTransform = ResolveBoneTransform(
			node, nodeName, entity, modelComp, ecsManager);
		if (boneTransform)
		{
			if (bakeParent && bakedParentTransform)
			{
				// SLOW PATH: We must multiply and decompose
				glm::mat4 matrixToApply = MultiplyAffine(*bakedParentTransform, nodeTransform);
				glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
				glm::decompose(matrixToApply, scale, rotation, translation, skew, perspective);

				Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
				TransformSystem::SetLocalTransform(
					*boneTransform,
					Vector3D::ConvertGLMToVector3D(translation),
					engineRot,
					Vector3D::ConvertGLMToVector3D(scale),
					Matrix4x4::ConvertToMatrix4x4(matrixToApply));
			}
			else
			{
				// FAST PATH
				if (hasBlendedTRS)
				{
					// We already have the math! Just plug it straight into the engine.
					Quaternion engineRot(finalRot.w, finalRot.x, finalRot.y, finalRot.z);
					TransformSystem::SetLocalTransform(
						*boneTransform,
						Vector3D::ConvertGLMToVector3D(finalPos),
						engineRot,
						Vector3D::ConvertGLMToVector3D(finalScale),
						Matrix4x4::ConvertToMatrix4x4(nodeTransform)
					);
				}
				else
				{
					Quaternion engineRot(
						node->bindRotation.w,
						node->bindRotation.x,
						node->bindRotation.y,
						node->bindRotation.z);
					TransformSystem::SetLocalTransform(
						*boneTransform,
						Vector3D::ConvertGLMToVector3D(node->bindTranslation),
						engineRot,
						Vector3D::ConvertGLMToVector3D(node->bindScale),
						Matrix4x4::ConvertToMatrix4x4(nodeTransform));
				}
			}
		}
	}

	// Keep skin-space transforms through the recursion. Applying the constant
	// global inverse once at the root removes one affine multiply per child bone.
	glm::mat4 rootHierarchyTransform;
	glm::mat4 skinTransformation;
	if (isRoot) {
		rootHierarchyTransform = MultiplyAffine(parentTransform, nodeTransform);
		skinTransformation = MultiplyAffine(globalInverse, rootHierarchyTransform);
	}
	else {
		skinTransformation = MultiplyAffine(parentTransform, nodeTransform);
	}

	// 4. Update shader matrices
	const BoneInfo* boneInfo = node->boneInfo;
	if (isRoot)
	{
		auto infoIt = boneInfoMap.find(nodeName);
		boneInfo = infoIt != boneInfoMap.end() ? &infoIt->second : nullptr;
	}
	if (boneInfo)
	{
		modelComp.mFinalBoneMatrices[boneInfo->id] =
			MultiplyAffine(skinTransformation, boneInfo->offset);
	}

	// 5. Recurse into children
	for (int i = 0; i < node->childrenCount; i++)
	{
		bool shouldChildBake = isRoot;
		CalculateBlendedBoneTransformInternal(&node->children[i], skinTransformation, entity, shouldChildBake,
			isRoot ? &rootHierarchyTransform : nullptr,
			ecsManager, modelComp, rootName, boneInfoMap, globalInverse, blendFactor);
	}
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent)
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	const std::uint64_t transformSetVersion =
		ecsManager.transformSystem->entities.Version();
	if (mBoneEntityCacheOwner != entity ||
		mTransformSetVersion != transformSetVersion) {
		ResetBoneEntityCache(entity);
		mTransformSetVersion = transformSetVersion;
	}
	mBoneEntityCacheCursor = 0;

	auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
	const std::string& rootName = ecsManager.GetComponent<NameComponent>(entity).name;
    CalculateBoneTransformInternal(node, parentTransform, entity, bakeParent, nullptr,
        ecsManager,
		modelComp,
		rootName,
        mCurrentAnimation->GetBoneIDMap(),
        mCurrentAnimation->GetGlobalInverse());
	++modelComp.bonePoseRevision;
}

void Animator::CalculateBoneTransformInternal(const AssimpNodeData* node, glm::mat4 parentTransform, Entity entity, bool bakeParent,
	const glm::mat4* bakedParentTransform,
    ECSManager& ecsManager, ModelRenderComponent& modelComp, const std::string& rootName,
	const std::map<std::string, BoneInfo>& boneInfoMap, const glm::mat4& globalInverse)
{
    bool isRoot = (node == &mCurrentAnimation->GetRootNode());

    const std::string& nodeName = isRoot
        ? rootName
        : node->name;

    glm::mat4 nodeTransform = node->transformation; // Default Bind Pose

    // 1. Calculate Animation Matrix
    Bone* bone = isRoot ? mCurrentAnimation->FindBone(nodeName) : node->animationBone;
    if (bone)
    {
        bone->Update(mCurrentTime);
        nodeTransform = bone->GetLocalTransform();
    }

	// The root entity owns the model transform and must not receive a bone-local
	// pose. Cache stable component pointers for all actual bone entities.
	if (!isRoot)
	{
		Transform* boneTransform = ResolveBoneTransform(
			node, nodeName, entity, modelComp, ecsManager);
		if (boneTransform)
		{
			if (bakeParent && bakedParentTransform)
			{
				// SLOW PATH: We must multiply and decompose
				glm::mat4 matrixToApply = MultiplyAffine(*bakedParentTransform, nodeTransform);
				glm::vec3 scale; glm::quat rotation; glm::vec3 translation; glm::vec3 skew; glm::vec4 perspective;
				glm::decompose(matrixToApply, scale, rotation, translation, skew, perspective);

				Quaternion engineRot(rotation.w, rotation.x, rotation.y, rotation.z);
				TransformSystem::SetLocalTransform(
					*boneTransform,
					Vector3D::ConvertGLMToVector3D(translation),
					engineRot,
					Vector3D::ConvertGLMToVector3D(scale),
					Matrix4x4::ConvertToMatrix4x4(matrixToApply));
			}
			else
			{
				// FAST PATH: Skip matrix math completely! Get TRS directly from the Bone.
				if (bone)
				{
					// The bone was animated this frame. Use its direct values.
					Quaternion engineRot(bone->GetLocalRotation().w, bone->GetLocalRotation().x, bone->GetLocalRotation().y, bone->GetLocalRotation().z);

					TransformSystem::SetLocalTransform(
						*boneTransform,
						Vector3D::ConvertGLMToVector3D(bone->GetLocalPosition()),
						engineRot,
						Vector3D::ConvertGLMToVector3D(bone->GetLocalScale()),
						Matrix4x4::ConvertToMatrix4x4(nodeTransform)
					);
				}
				else
				{
					Quaternion engineRot(
						node->bindRotation.w,
						node->bindRotation.x,
						node->bindRotation.y,
						node->bindRotation.z);
					TransformSystem::SetLocalTransform(
						*boneTransform,
						Vector3D::ConvertGLMToVector3D(node->bindTranslation),
						engineRot,
						Vector3D::ConvertGLMToVector3D(node->bindScale),
						Matrix4x4::ConvertToMatrix4x4(nodeTransform));
				}
			}
	        }
	    }

    // Keep skin-space transforms through the recursion. Applying the constant
    // global inverse once at the root removes one affine multiply per child bone.
	glm::mat4 rootHierarchyTransform;
	glm::mat4 skinTransformation;
	if (isRoot) {
		rootHierarchyTransform = MultiplyAffine(parentTransform, nodeTransform);
		skinTransformation = MultiplyAffine(globalInverse, rootHierarchyTransform);
	}
	else {
		skinTransformation = MultiplyAffine(parentTransform, nodeTransform);
	}

    // 5. Update Shader Matrices
	const BoneInfo* boneInfo = node->boneInfo;
	if (isRoot)
	{
		auto infoIt = boneInfoMap.find(nodeName);
		boneInfo = infoIt != boneInfoMap.end() ? &infoIt->second : nullptr;
	}
    if (boneInfo)
    {
        modelComp.mFinalBoneMatrices[boneInfo->id] =
			MultiplyAffine(skinTransformation, boneInfo->offset);
    }

    // 6. Recurse
    for (int i = 0; i < node->childrenCount; i++)
    {
        bool shouldChildBake = isRoot;
        CalculateBoneTransformInternal(&node->children[i], skinTransformation, entity, shouldChildBake,
			isRoot ? &rootHierarchyTransform : nullptr,
            ecsManager, modelComp, rootName, boneInfoMap, globalInverse);
    }
}
