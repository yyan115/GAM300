#pragma once
#include <glm/gtc/quaternion.hpp>
#include <pch.h>
#include <assimp/anim.h>

struct KeyPosition
{
    glm::vec3 position;
    float timeStamp;
};

struct KeyRotation
{
    glm::quat orientation;
    float timeStamp;
};

struct KeyScale
{
    glm::vec3 scale;
    float timeStamp;
};


class Bone
{
private:
    std::vector<KeyPosition> mPositions;
    std::vector<KeyRotation> mRotations;
    std::vector<KeyScale> mScales;
    int mNumPositions;
    int mNumRotations;
    int mNumScalings;

    glm::mat4 mLocalTransform;
    std::string mName;
    int mID;

public:
    /*reads keyframes from aiNodeAnim*/
    Bone(const std::string& name, int ID, const aiNodeAnim* channel);

	void Update(float animationTime);

    glm::mat4 GetLocalTransform() const { return mLocalTransform; }
    std::string GetBoneName() const { return mName; }
	int GetBoneID() const { return mID; }

    int GetPositionIndex(float animationTime);
    int GetRotationIndex(float animationTime);
    int GetScaleIndex(float animationTime);

private:
    float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime);
    glm::mat4 InterpolatePosition(float animationTime);
    glm::mat4 InterpolateRotation(float animationTime);
	glm::mat4 InterpolateScaling(float animationTime);

};