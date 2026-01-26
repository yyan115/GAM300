#include <pch.h>
#include "Animation/Animation.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

static std::string NormalizeFbxName(std::string n);
static  bool IsAssimpFbxTrsNode(const std::string& n);
glm::mat4 aiToGlm(const aiMatrix4x4& from);
glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4& from);

//static void DebugMatrix(const char* tag, const glm::mat4& M)
//{
//	ENGINE_LOG_DEBUG(std::string(tag));
//	for (int r = 0; r < 4; ++r) {
//		for (int c = 0; c < 4; ++c) {
//			ENGINE_LOG_DEBUG(std::to_string(M[c][r]) + " ");
//		}
//	}
//}
//
//void Animation::DebugCoreMatricesOnce() const
//{
//	DebugMatrix("[Anim] GlobalInverse", mGlobalInverse);
//	auto itH = mBoneInfoMap.find("mixamorig:Hips");
//	auto itF = mBoneInfoMap.find("mixamorig:LeftFoot");
//	if (itH != mBoneInfoMap.end()) DebugMatrix("[Anim] Offset(Hips)", itH->second.offset);
//	if (itF != mBoneInfoMap.end()) DebugMatrix("[Anim] Offset(LeftFoot)", itF->second.offset);
//}

Animation::Animation(aiAnimation* animation, const aiNode* rootNode, std::map<std::string, BoneInfo> boneInfoMap, int boneCount) 
{
	mDuration = static_cast<float>(animation->mDuration);
	mTicksPerSecond = animation->mTicksPerSecond != 0.0 ? static_cast<int>(animation->mTicksPerSecond) : 25;

	mGlobalInverse = glm::inverse(aiMatrix4x4ToGlm(rootNode->mTransformation));
	
	//const aiMatrix4x4& rootTrf = rootNode->mTransformation;
	//ENGINE_LOG_DEBUG("[RootTransform] [" +
	//	std::to_string(rootTrf.a1) + " " + std::to_string(rootTrf.b1) + " " + std::to_string(rootTrf.c1) + " " + std::to_string(rootTrf.d1) + "] [" +
	//	std::to_string(rootTrf.a2) + " " + std::to_string(rootTrf.b2) + " " + std::to_string(rootTrf.c2) + " " + std::to_string(rootTrf.d2) + "] [" +
	//	std::to_string(rootTrf.a3) + " " + std::to_string(rootTrf.b3) + " " + std::to_string(rootTrf.c3) + " " + std::to_string(rootTrf.d3) + "] [" +
	//	std::to_string(rootTrf.a4) + " " + std::to_string(rootTrf.b4) + " " + std::to_string(rootTrf.c4) + " " + std::to_string(rootTrf.d4) + "]\n");

	//ENGINE_LOG_DEBUG("[Anim] Global Inverse Matrix:\n");
	//for (int row = 0; row < 4; ++row)
	//{
	//	for (int col = 0; col < 4; ++col)
	//	{
	//		ENGINE_LOG_DEBUG(std::to_string(mGlobalInverse[col][row]));
	//	}
	//}

	//ENGINE_LOG_DEBUG("[GlobalInverse] [" +
	//	std::to_string(mGlobalInverse[0][0]) + " " + std::to_string(mGlobalInverse[1][0]) + " " + std::to_string(mGlobalInverse[2][0]) + " " + std::to_string(mGlobalInverse[3][0]) + "] [" +
	//	std::to_string(mGlobalInverse[0][1]) + " " + std::to_string(mGlobalInverse[1][1]) + " " + std::to_string(mGlobalInverse[2][1]) + " " + std::to_string(mGlobalInverse[3][1]) + "] [" +
	//	std::to_string(mGlobalInverse[0][2]) + " " + std::to_string(mGlobalInverse[1][2]) + " " + std::to_string(mGlobalInverse[2][2]) + " " + std::to_string(mGlobalInverse[3][2]) + "] [" +
	//	std::to_string(mGlobalInverse[0][3]) + " " + std::to_string(mGlobalInverse[1][3]) + " " + std::to_string(mGlobalInverse[2][3]) + " " + std::to_string(mGlobalInverse[3][3]) + "]\n");

	ReadHierarchyData(mRootNode, rootNode);
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

	//ENGINE_LOG_DEBUG("[ReadMissingBones] Animation has " + std::to_string(num) + " channels\n");
	//ENGINE_LOG_DEBUG("[ReadMissingBones] Incoming boneInfoMap has " + std::to_string(boneInfoMap.size()) + " bones\n");

	for (int i = 0; i < num; ++i)
	{
		const aiNodeAnim* channel = animation->mChannels[i];
		const std::string boneName = NormalizeFbxName(channel->mNodeName.C_Str());

		// Ensure the bone exists in the info map and has an ID
		auto it = boneInfoMap.find(boneName);
		if (it == boneInfoMap.end())
		{
			//ENGINE_LOG_WARN("[ReadMissingBones] Bone '" + boneName + "' NOT FOUND in model's boneInfoMap! Creating default.\n");

			BoneInfo info;
			info.id = boneCount++;
			info.offset = glm::mat4(1.0f); // default if not in mesh
			it = boneInfoMap.emplace(boneName, info).first;
		}
		//else {
		//	if (boneName == "mixamorig:Hips" || boneName == "mixamorig:Spine") {
		//		ENGINE_LOG_DEBUG("[ReadMissingBones] ✓ Bone '" + boneName + "' FOUND in model map, ID=" + std::to_string(it->second.id) + "\n");
		//	}
		//}

		mBones.emplace_back(boneName, it->second.id, channel);
	}

	mBoneInfoMap = std::move(boneInfoMap);

	//// FINAL CHECK: Log what ended up in mBoneInfoMap
	//ENGINE_LOG_DEBUG("[ReadMissingBones] Final mBoneInfoMap has " + std::to_string(mBoneInfoMap.size()) + " bones\n");
	//for (const auto& [name, info] : mBoneInfoMap) {
	//	if (name == "mixamorig:Hips" || name == "mixamorig:Spine") {
	//		ENGINE_LOG_DEBUG("[ReadMissingBones-Final] '" + name + "' ID=" + std::to_string(info.id) + " Offset: [" +
	//			std::to_string(info.offset[0][0]) + " " + std::to_string(info.offset[1][0]) + " " + std::to_string(info.offset[2][0]) + " " + std::to_string(info.offset[3][0]) + "] [" +
	//			std::to_string(info.offset[0][1]) + " " + std::to_string(info.offset[1][1]) + " " + std::to_string(info.offset[2][1]) + " " + std::to_string(info.offset[3][1]) + "] [" +
	//			std::to_string(info.offset[0][2]) + " " + std::to_string(info.offset[1][2]) + " " + std::to_string(info.offset[2][2]) + " " + std::to_string(info.offset[3][2]) + "] [" +
	//			std::to_string(info.offset[0][3]) + " " + std::to_string(info.offset[1][3]) + " " + std::to_string(info.offset[2][3]) + " " + std::to_string(info.offset[3][3]) + "]\n");
	//	}
	//}
}

void Animation::ReadHierarchyData(AssimpNodeData& dest, const aiNode* src, glm::mat4 accum)
{
	assert(src);

	std::string rawName = src->mName.C_Str();
	glm::mat4 local = aiMatrix4x4ToGlm(src->mTransformation);

	if(IsAssimpFbxTrsNode(rawName))
	{
		glm::mat4 nextAccum = accum * local;
		for (unsigned int i = 0; i < src->mNumChildren; i++)
		{
			ReadHierarchyData(dest, src->mChildren[i], nextAccum);
		}
		return;
	}

	dest.name = NormalizeFbxName(rawName);
	dest.transformation = accum * local;

	//// LOG KEY BONES
	//if (dest.name == "mixamorig:Hips" || dest.name == "mixamorig:Spine") {
	//	ENGINE_LOG_DEBUG("[Hierarchy] Bone '" + dest.name + "' transform: [" +
	//		std::to_string(dest.transformation[0][0]) + " " + std::to_string(dest.transformation[1][0]) + " " + std::to_string(dest.transformation[2][0]) + " " + std::to_string(dest.transformation[3][0]) + "] [" +
	//		std::to_string(dest.transformation[0][1]) + " " + std::to_string(dest.transformation[1][1]) + " " + std::to_string(dest.transformation[2][1]) + " " + std::to_string(dest.transformation[3][1]) + "] [" +
	//		std::to_string(dest.transformation[0][2]) + " " + std::to_string(dest.transformation[1][2]) + " " + std::to_string(dest.transformation[2][2]) + " " + std::to_string(dest.transformation[3][2]) + "] [" +
	//		std::to_string(dest.transformation[0][3]) + " " + std::to_string(dest.transformation[1][3]) + " " + std::to_string(dest.transformation[2][3]) + " " + std::to_string(dest.transformation[3][3]) + "]\n");
	//}

	dest.children.clear();
	dest.childrenCount = 0;

	for (unsigned int i = 0; i < src->mNumChildren; i++)
	{
		AssimpNodeData newData{};
		ReadHierarchyData(newData, src->mChildren[i]);
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
	//const char* tag = "_$AssimpFbx$";
	//size_t p = n.find(tag);
	//if (p != std::string::npos) n.erase(p);  // strip suffix, keep "mixamorig:Spine"
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
