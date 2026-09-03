#pragma once
#include <glm/glm.hpp>
#include <Graphics/Bone.hpp>
#include <pch.h>
#include <assimp/scene.h>
#include "Graphics/Model/BoneInfo.hpp"
#include "Asset Manager/Asset.hpp"
#include <cstdint>

struct AssimpNodeData
{
    glm::mat4 transformation{};
    glm::vec3 bindTranslation{ 0.0f };
    glm::quat bindRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 bindScale{ 1.0f };
    std::string name{};
    int childrenCount{};
    std::vector<AssimpNodeData> children{};
    Bone* animationBone = nullptr;
    const BoneInfo* boneInfo = nullptr;
};

class Animation : public IAsset
{
private:
    float mDuration{};
    int mTicksPerSecond{};
    std::vector<Bone> mBones{};
    std::unordered_map<std::string, Bone*> mBoneLookup{}; // O(1) bone lookup cache
    std::map<std::string, BoneInfo> mBoneInfoMap{};
    AssimpNodeData mRootNode{};
    glm::mat4 mGlobalInverse{};
    std::uint64_t mRevision = 0;

public:
	
    Animation() = default;

    Animation(aiAnimation* animation, const aiNode* rootNode, std::map<std::string, BoneInfo> boneInfoMap, int boneCount);

	~Animation() = default;

    std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
    bool LoadResource(const std::string& resourcePath, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount);
    bool ReloadResource(const std::string& resourcePath, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount);
    std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;

    Bone* FindBone(const std::string& name);
    inline float GetTicksPerSecond() const { return (float)mTicksPerSecond; }
    inline float GetDuration() const { return mDuration; }
    inline const AssimpNodeData& GetRootNode() const { return mRootNode; }
	inline const std::map<std::string, BoneInfo>& GetBoneIDMap() const { return mBoneInfoMap; }
	const glm::mat4& GetGlobalInverse() const { return mGlobalInverse; }
    std::uint64_t GetRevision() const { return mRevision; }
    void DebugCoreMatricesOnce() const;

private:
	void ReadMissingBones(const aiAnimation* animation, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount);

	void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src, glm::mat4 accTrf = glm::mat4(1.0f));
    void BindHierarchyData(AssimpNodeData& node);

};
