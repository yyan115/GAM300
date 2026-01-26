#include <pch.h>
#include "Graphics/Bone.hpp"
#include <Logging.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

Bone::Bone(const std::string& name, int ID, const aiNodeAnim* channel)
    : mName(name),
    mID(ID),
    mLocalTransform(1.0f)
{
    mNumPositions = channel->mNumPositionKeys;
    mNumRotations = channel->mNumRotationKeys;
    mNumScalings = channel->mNumScalingKeys;

    //// Log keyframe counts and first keyframe for key bones
    //if (name == "mixamorig:Hips" || name == "mixamorig:Spine") {
    //    ENGINE_LOG_DEBUG("[Bone Constructor] '" + name + "' ID=" + std::to_string(ID) + "\n");
    //    ENGINE_LOG_DEBUG("  NumPositionKeys=" + std::to_string(mNumPositions) + "\n");
    //    ENGINE_LOG_DEBUG("  NumRotationKeys=" + std::to_string(mNumRotations) + "\n");
    //    ENGINE_LOG_DEBUG("  NumScalingKeys=" + std::to_string(mNumScalings) + "\n");

    //    if (mNumPositions > 0) {
    //        const aiVectorKey& firstPos = channel->mPositionKeys[0];
    //        ENGINE_LOG_DEBUG("  First position key: [" +
    //            std::to_string(firstPos.mValue.x) + ", " +
    //            std::to_string(firstPos.mValue.y) + ", " +
    //            std::to_string(firstPos.mValue.z) + "] at time " +
    //            std::to_string(firstPos.mTime) + "\n");

    //        if (mNumPositions > 1) {
    //            const aiVectorKey& secondPos = channel->mPositionKeys[1];
    //            ENGINE_LOG_DEBUG("  Second position key: [" +
    //                std::to_string(secondPos.mValue.x) + ", " +
    //                std::to_string(secondPos.mValue.y) + ", " +
    //                std::to_string(secondPos.mValue.z) + "] at time " +
    //                std::to_string(secondPos.mTime) + "\n");
    //        }
    //    }

    //    if (mNumRotations > 0) {
    //        const aiQuatKey& firstRot = channel->mRotationKeys[0];
    //        ENGINE_LOG_DEBUG("  First rotation key: [" +
    //            std::to_string(firstRot.mValue.w) + ", " +
    //            std::to_string(firstRot.mValue.x) + ", " +
    //            std::to_string(firstRot.mValue.y) + ", " +
    //            std::to_string(firstRot.mValue.z) + "] at time " +
    //            std::to_string(firstRot.mTime) + "\n");
    //    }
    //}

    for (int positionIndex = 0; positionIndex < mNumPositions; ++positionIndex)
    {
        aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
        float timeStamp = static_cast<float>(channel->mPositionKeys[positionIndex].mTime);
        KeyPosition data;
        data.position = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);
        data.timeStamp = timeStamp;
        mPositions.push_back(data);
    }

    for (int rotationIndex = 0; rotationIndex < mNumRotations; ++rotationIndex)
    {
        aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
        float timeStamp = static_cast<float>(channel->mRotationKeys[rotationIndex].mTime);
        KeyRotation data;
        data.orientation = glm::quat(aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z);
        data.timeStamp = timeStamp;
        mRotations.push_back(data);
    }

    for (int keyIndex = 0; keyIndex < mNumScalings; ++keyIndex)
    {
        aiVector3D scale = channel->mScalingKeys[keyIndex].mValue;
        float timeStamp = static_cast<float>(channel->mScalingKeys[keyIndex].mTime);
        KeyScale data;
        data.scale = glm::vec3(scale.x, scale.y, scale.z);
        data.timeStamp = timeStamp;
        mScales.push_back(data);
    }
}

/*interpolates  b/w positions,rotations & scaling keys based on the curren time of
   the animation and prepares the local transformation matrix by combining all keys
   tranformations*/
void Bone::Update(float animationTime)
{
    mLocalTranslation = InterpolatePosition(animationTime);
    mLocalRotation = InterpolateRotation(animationTime);
    mLocalScale = InterpolateScaling(animationTime);
    mLocalTransform = glm::translate(glm::mat4(1.0f), mLocalTranslation) * glm::toMat4(mLocalRotation) * glm::scale(glm::mat4(1.0f), mLocalScale);

    //// Log for key bones at start of animation
    //if ((mName == "mixamorig:Hips" || mName == "mixamorig:Spine") && animationTime < 0.5f) {
    //    ENGINE_LOG_DEBUG("[Bone::Update] '" + mName + "' at time " + std::to_string(animationTime) +
    //        " LocalTransform: [" +
    //        std::to_string(mLocalTransform[0][0]) + " " + std::to_string(mLocalTransform[1][0]) + " " + std::to_string(mLocalTransform[2][0]) + " " + std::to_string(mLocalTransform[3][0]) + "] [" +
    //        std::to_string(mLocalTransform[0][1]) + " " + std::to_string(mLocalTransform[1][1]) + " " + std::to_string(mLocalTransform[2][1]) + " " + std::to_string(mLocalTransform[3][1]) + "] [" +
    //        std::to_string(mLocalTransform[0][2]) + " " + std::to_string(mLocalTransform[1][2]) + " " + std::to_string(mLocalTransform[2][2]) + " " + std::to_string(mLocalTransform[3][2]) + "] [" +
    //        std::to_string(mLocalTransform[0][3]) + " " + std::to_string(mLocalTransform[1][3]) + " " + std::to_string(mLocalTransform[2][3]) + " " + std::to_string(mLocalTransform[3][3]) + "]\n");
    //}
}

/* Gets the current index on mKeyPositions to interpolate to based on
	the current animation time*/
int Bone::GetPositionIndex(float animationTime)
{
	for (int index = 0; index < mNumPositions - 1; ++index)
	{
		if (animationTime < mPositions[index + 1].timeStamp)
			return index;
	}
	return mNumPositions - 2;
}

/* Gets the current index on mKeyRotations to interpolate to based on the
current animation time*/
int Bone::GetRotationIndex(float animationTime)
{
	for (int index = 0; index < mNumRotations - 1; ++index)
	{
		if (animationTime < mRotations[index + 1].timeStamp)
			return index;
	}
	return mNumRotations - 2;
}

/* Gets the current index on mKeyScalings to interpolate to based on the
current animation time */
int Bone::GetScaleIndex(float animationTime)
{
	for (int index = 0; index < mNumScalings - 1; ++index)
	{
		if (animationTime < mScales[index + 1].timeStamp)
			return index;
	}
	return mNumScalings - 2;
}


/* Gets normalized value for Lerp & Slerp*/
float Bone::GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
{
	float scaleFactor = 0.0f;
	float midWayLength = animationTime - lastTimeStamp;
	float framesDiff = nextTimeStamp - lastTimeStamp;
	scaleFactor = midWayLength / framesDiff;
	return scaleFactor;
}

/*figures out which position keys to interpolate b/w and performs the interpolation
	and returns the translation matrix*/
glm::vec3 Bone::InterpolatePosition(float animationTime)
{
	if (1 == mNumPositions)
		return mPositions[0].position;

	int p0Index = GetPositionIndex(animationTime);
	int p1Index = p0Index + 1;

	float scaleFactor = GetScaleFactor(mPositions[p0Index].timeStamp, mPositions[p1Index].timeStamp, animationTime);
	glm::vec3 finalPosition = glm::mix(mPositions[p0Index].position, mPositions[p1Index].position, scaleFactor);

	return finalPosition;
}

/*figures out which rotations keys to interpolate b/w and performs the interpolation
	and returns the rotation matrix*/
glm::quat Bone::InterpolateRotation(float animationTime)
{
	if (1 == mNumRotations)
	{
		return glm::normalize(mRotations[0].orientation);
	}

	int r0Index = GetRotationIndex(animationTime);
	int r1Index = r0Index + 1;
    float scaleFactor = GetScaleFactor(mRotations[r0Index].timeStamp, mRotations[r1Index].timeStamp, animationTime);
	glm::quat finalRotation = glm::slerp(mRotations[r0Index].orientation, mRotations[r1Index].orientation, scaleFactor);

	finalRotation = glm::normalize(finalRotation);
	
    return finalRotation;
}

/*figures out which scaling keys to interpolate b/w and performs the interpolation
	and returns the scale matrix*/
glm::vec3 Bone::InterpolateScaling(float animationTime)
{
	if (1 == mNumScalings)
		return mScales[0].scale;

	int s0Index = GetScaleIndex(animationTime);
	int s1Index = s0Index + 1;

	float scaleFactor = GetScaleFactor(mScales[s0Index].timeStamp, mScales[s1Index].timeStamp, animationTime);
	glm::vec3 finalScale = glm::mix(mScales[s0Index].scale, mScales[s1Index].scale, scaleFactor);

	return finalScale;
}