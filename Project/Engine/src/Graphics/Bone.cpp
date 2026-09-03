#include <pch.h>
#include "Graphics/Bone.hpp"
#include <Logging.hpp>
#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace {
constexpr float kMinKeyframeGap = 1.0e-6f;

glm::mat4 ComposeTRS(
    const glm::vec3& translation,
    const glm::quat& rotation,
    const glm::vec3& scale)
{
    glm::mat4 result = glm::toMat4(rotation);
    result[0] *= scale.x;
    result[1] *= scale.y;
    result[2] *= scale.z;
    result[3] = glm::vec4(translation, 1.0f);
    return result;
}

bool IsFiniteVec3(const glm::vec3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFiniteQuat(const glm::quat& value)
{
    return std::isfinite(value.w) && std::isfinite(value.x) &&
        std::isfinite(value.y) && std::isfinite(value.z);
}

glm::quat SafeNormalizeQuat(const glm::quat& value, const glm::quat& fallback = glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
{
    if (!IsFiniteQuat(value)) {
        return fallback;
    }

    const float lenSq = value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z;
    if (!(lenSq > kMinKeyframeGap)) {
        return fallback;
    }

    // Imported keys are normalized once and GLM's slerp preserves unit length
    // to normal floating-point precision. Keep the defensive slow path for bad
    // data without renormalizing every animated bone on every frame.
    if (std::fabs(lenSq - 1.0f) <= 1.0e-5f) {
        return value;
    }

    const glm::quat normalized = glm::normalize(value);
    return IsFiniteQuat(normalized) ? normalized : fallback;
}

float ClampBlendFactor(float value)
{
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

template <typename Key>
int FindKeyIndex(const std::vector<Key>& keys, float animationTime, int& cachedIndex)
{
    // Callers handle single-key channels, but guard anyway: std::clamp with
    // an inverted range is undefined behaviour.
    if (keys.size() < 2) {
        cachedIndex = 0;
        return 0;
    }
    const int lastInterval = static_cast<int>(keys.size()) - 2;
    cachedIndex = std::clamp(cachedIndex, 0, lastInterval);

    if (!std::isfinite(animationTime)) {
        cachedIndex = 0;
        return cachedIndex;
    }

    if (animationTime >= keys[cachedIndex].timeStamp &&
        animationTime < keys[cachedIndex + 1].timeStamp) {
        return cachedIndex;
    }

    // Playback normally advances by less than one key interval per frame. Walk
    // the cached cursor forward in that common case and reserve binary search
    // for loops, editor seeks, and other backward jumps.
    if (animationTime >= keys[cachedIndex + 1].timeStamp) {
        while (cachedIndex < lastInterval &&
            animationTime >= keys[cachedIndex + 1].timeStamp) {
            ++cachedIndex;
        }
        return cachedIndex;
    }

    const auto upper = std::upper_bound(
        keys.begin() + 1,
        keys.end(),
        animationTime,
        [](float time, const Key& key) { return time < key.timeStamp; });
    cachedIndex = std::clamp(
        static_cast<int>(std::distance(keys.begin(), upper)) - 1,
        0,
        lastInterval);
    return cachedIndex;
}
}

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
        data.orientation = SafeNormalizeQuat(
            glm::quat(aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z));
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
    mLocalTransform =
        ComposeTRS(mLocalTranslation, mLocalRotation, mLocalScale);

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
    return FindKeyIndex(mPositions, animationTime, mLastPositionIndex);
}

/* Gets the current index on mKeyRotations to interpolate to based on the
current animation time*/
int Bone::GetRotationIndex(float animationTime)
{
    return FindKeyIndex(mRotations, animationTime, mLastRotationIndex);
}

/* Gets the current index on mKeyScalings to interpolate to based on the
current animation time */
int Bone::GetScaleIndex(float animationTime)
{
    return FindKeyIndex(mScales, animationTime, mLastScaleIndex);
}


/* Gets normalized value for Lerp & Slerp*/
float Bone::GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
{
    if (!std::isfinite(lastTimeStamp) || !std::isfinite(nextTimeStamp) || !std::isfinite(animationTime)) {
        return 0.0f;
    }

	float midWayLength = animationTime - lastTimeStamp;
	float framesDiff = nextTimeStamp - lastTimeStamp;
    if (std::fabs(framesDiff) <= kMinKeyframeGap) {
        return 0.0f;
    }

	return ClampBlendFactor(midWayLength / framesDiff);
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
    if (!IsFiniteVec3(finalPosition)) {
        return mPositions[p0Index].position;
    }

	return finalPosition;
}

/*figures out which rotations keys to interpolate b/w and performs the interpolation
	and returns the rotation matrix*/
glm::quat Bone::InterpolateRotation(float animationTime)
{
	if (1 == mNumRotations)
	{
		return mRotations[0].orientation;
	}

	int r0Index = GetRotationIndex(animationTime);
	int r1Index = r0Index + 1;
    float scaleFactor = GetScaleFactor(mRotations[r0Index].timeStamp, mRotations[r1Index].timeStamp, animationTime);
    const glm::quat& startRotation = mRotations[r0Index].orientation;
    const glm::quat& endRotation = mRotations[r1Index].orientation;
	glm::quat finalRotation = glm::slerp(startRotation, endRotation, scaleFactor);

	finalRotation = SafeNormalizeQuat(finalRotation, startRotation);
	
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
    if (!IsFiniteVec3(finalScale)) {
        return mScales[s0Index].scale;
    }

	return finalScale;
}
