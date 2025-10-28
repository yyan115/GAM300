#include <pch.h>
#include "Graphics/Bone.hpp"

Bone::Bone(const std::string& name, int ID, const aiNodeAnim* channel)
	: mName(name), mID(ID), mLocalTransform(1.0f)
{
	mNumPositions = channel->mNumPositionKeys;

	for (int positionIndex = 0; positionIndex < mNumPositions; ++positionIndex)
	{
		aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
		float timeStamp = static_cast<float>(channel->mPositionKeys[positionIndex].mTime);
		KeyPosition data;
		data.position = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);
		data.timeStamp = timeStamp;
		mPositions.push_back(data);
	}

	mNumRotations = channel->mNumRotationKeys;

	for (int rotationIndex = 0; rotationIndex < mNumRotations; ++rotationIndex)
	{
		aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
		float timeStamp = static_cast<float>(channel->mRotationKeys[rotationIndex].mTime);
		KeyRotation data;
		data.orientation = glm::quat(aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z);
		data.timeStamp = timeStamp;
		mRotations.push_back(data);
	}

	mNumScalings = channel->mNumScalingKeys;

	for (int scaleIndex = 0; scaleIndex < mNumScalings; ++scaleIndex)
	{
		aiVector3D scale = channel->mScalingKeys[scaleIndex].mValue;
		float timeStamp = static_cast<float>(channel->mScalingKeys[scaleIndex].mTime);
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
	glm::mat4 translation = InterpolatePosition(animationTime);
	glm::mat4 rotation = InterpolateRotation(animationTime);
	glm::mat4 scale = InterpolateScaling(animationTime);
	mLocalTransform = translation * rotation * scale;
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
	assert(0);
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
	assert(0);
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
	assert(0);
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
glm::mat4 Bone::InterpolatePosition(float animationTime)
{
	if (1 == mNumPositions)
		return glm::translate(glm::mat4(1.0f), mPositions[0].position);

	int p0Index = GetPositionIndex(animationTime);
	int p1Index = p0Index + 1;

	float scaleFactor = GetScaleFactor(mPositions[p0Index].timeStamp, mPositions[p1Index].timeStamp, animationTime);
	glm::vec3 finalPosition = glm::mix(mPositions[p0Index].position, mPositions[p1Index].position, scaleFactor);

	return glm::translate(glm::mat4(1.0f), finalPosition);
}

/*figures out which rotations keys to interpolate b/w and performs the interpolation
	and returns the rotation matrix*/
glm::mat4 Bone::InterpolateRotation(float animationTime)
{
	if (1 == mNumRotations)
	{
		auto rotation = glm::normalize(mRotations[0].orientation);
		return glm::mat4_cast(rotation);
	}

	int r0Index = GetRotationIndex(animationTime);
	int r1Index = r0Index + 1;
	float scaleFactor = GetScaleFactor(mRotations[r0Index].timeStamp, mRotations[r1Index].timeStamp, animationTime);
	glm::quat finalRotation = glm::slerp(mRotations[r0Index].orientation, mRotations[r1Index].orientation, scaleFactor);

	finalRotation = glm::normalize(finalRotation);
	
	return glm::mat4_cast(finalRotation);
}

/*figures out which scaling keys to interpolate b/w and performs the interpolation
	and returns the scale matrix*/
glm::mat4 Bone::InterpolateScaling(float animationTime)
{
	if (1 == mNumScalings)
		return glm::scale(glm::mat4(1.0f), mScales[0].scale);

	int s0Index = GetScaleIndex(animationTime);
	int s1Index = s0Index + 1;

	float scaleFactor = GetScaleFactor(mScales[s0Index].timeStamp, mScales[s1Index].timeStamp, animationTime);
	glm::vec3 finalScale = glm::mix(mScales[s0Index].scale, mScales[s1Index].scale, scaleFactor);

	return glm::scale(glm::mat4(1.0f), finalScale);
}