#pragma once
#include <glm/glm.hpp>
#include <Graphics/Bone.hpp>
#include <pch.h>
#include <assimp/scene.h>
#include "Graphics/Model/Model.h"

struct AssimpNodeData
{
    glm::mat4 transformation{};
    std::string name{};
    int childrenCount{};
    std::vector<AssimpNodeData> children{};
};

class Animation
{
private:
    float mDuration{};
    int mTicksPerSecond{};
    std::vector<Bone> mBones{};
    std::map<std::string, BoneInfo> mBoneInfoMap{};
    AssimpNodeData mRootNode{};
    glm::mat4 mGlobalInverse{};

public:
	
    Animation() = default;

    Animation(aiAnimation* animation, const aiNode* rootNode, std::map<std::string, BoneInfo> boneInfoMap, int boneCount);

	~Animation() = default;

    Bone* FindBone(const std::string& name);
    inline float GetTicksPerSecond() { return (float)mTicksPerSecond; }
    inline float GetDuration() { return mDuration; }
    inline const AssimpNodeData& GetRootNode() { return mRootNode; }
	inline const std::map<std::string, BoneInfo>& GetBoneIDMap() { return mBoneInfoMap; }
	glm::mat4 GetGlobalInverse() { return mGlobalInverse; }


private:
	void ReadMissingBones(const aiAnimation* animation, std::map<std::string, BoneInfo> boneInfoMap, int boneCount);

	void ReadHeirarchyData(AssimpNodeData& dest, const aiNode* src, glm::mat4 accTrf = glm::mat4(1.0f));

};