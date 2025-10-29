#include <pch.h>
#include "Animation/Animation.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

static std::string NormalizeFbxName(std::string n);
static  bool IsAssimpFbxTrsNode(const std::string& n);
glm::mat4 aiToGlm(const aiMatrix4x4& from);
glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& from);

Animation::Animation(aiAnimation* animation, const aiNode* rootNode, std::map<std::string, BoneInfo> boneInfoMap, int boneCount) 
{
	mDuration = animation->mDuration;
	mTicksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25;

	mGlobalInverse = glm::inverse(aiToGlm(rootNode->mTransformation));
	//mGlobalInverse = aiToGlm(rootNode->mTransformation);

	ReadHeirarchyData(mRootNode, rootNode);
	ReadMissingBones(animation, boneInfoMap, boneCount);
}

Bone* Animation::FindBone(const std::string& name)
{
	auto iter = std::find_if(mBones.begin(), mBones.end(),
		[&](const Bone& Bone)
		{
			return Bone.GetBoneName() == name;
		}
	);
	if (iter == mBones.end()) return nullptr;
	else return &(*iter);
}

void Animation::ReadMissingBones(const aiAnimation* animation,
	std::map<std::string, BoneInfo> boneInfoMap,
	int boneCount)
{
	const int num = animation ? static_cast<int>(animation->mNumChannels) : 0;
	mBones.reserve(mBones.size() + num);

	for (int i = 0; i < num; ++i)
	{
		const aiNodeAnim* channel = animation->mChannels[i];
		// Normalize once
		const std::string boneName = NormalizeFbxName(channel->mNodeName.C_Str());

		// Ensure the bone exists in the info map and has an ID
		auto it = boneInfoMap.find(boneName);
		if (it == boneInfoMap.end())
		{
			BoneInfo info;
			info.id = boneCount++;
			info.offset = glm::mat4(1.0f); // default if not in mesh
			it = boneInfoMap.emplace(boneName, info).first;
		}

		// This requires Bone(std::string,int,const aiNodeAnim*)
		mBones.emplace_back(boneName, it->second.id, channel);
	}

	mBoneInfoMap = std::move(boneInfoMap);
}


void Animation::ReadHeirarchyData(AssimpNodeData& dest, const aiNode* src, glm::mat4 accum)
{
	assert(src);

	std::string rawName = src->mName.C_Str();
	glm::mat4 local = aiMatrix4x4ToGlm(src->mTransformation);

	if(IsAssimpFbxTrsNode(rawName))
	{
		glm::mat4 nextAccum = accum * local;
		for (int i = 0; i < src->mNumChildren; i++)
		{
			ReadHeirarchyData(dest, src->mChildren[i], nextAccum);
		}
		return;
	}

	dest.name = NormalizeFbxName(rawName);
	dest.transformation = accum * local;
	dest.children.clear();
	dest.childrenCount = 0;

	for (int i = 0; i < src->mNumChildren; i++)
	{
		AssimpNodeData newData{};
		ReadHeirarchyData(newData, src->mChildren[i]);
		if (!newData.name.empty())
		{
			dest.children.push_back(std::move(newData));
			++dest.childrenCount;
		}
	}
	dest.childrenCount = static_cast<int>(dest.children.size());
}



static inline std::string NormalizeFbxName(std::string n)
{
	const char* tag = "_$AssimpFbx$";
	size_t p = n.find(tag);
	if (p != std::string::npos) n.erase(p);  // strip suffix, keep "mixamorig:Spine"
	return n;
}

static inline bool IsAssimpFbxTrsNode(const std::string& n) 
{
	return n.find("_$AssimpFbx$") != std::string::npos;
}

glm::mat4 aiToGlm(const aiMatrix4x4& m)
{
	return glm::mat4(
		m.a1, m.b1, m.c1, m.d1,
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
}

inline glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& from)
{
	glm::mat4 to;
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}
