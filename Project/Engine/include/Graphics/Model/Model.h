#pragma once
#include "Graphics/Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <algorithm>
#include "Asset Manager/Asset.hpp"
#include "../../Engine.h"

struct BoneInfo
{
	// Id is index in finalBoneMatrices
	int id;

	// Offset matrix transforms vertex from model space to bone space
	glm::mat4 offset;
};


class Model : public IAsset {
public:
	std::vector<Mesh> meshes;
	std::string directory;

	//Model(const std::string& filePath);
	ENGINE_API std::string CompileToResource(const std::string& assetPath) override;
	std::string CompileToMesh(const std::string& modelPath, const std::vector<Mesh>& meshesToCompile);
	ENGINE_API bool LoadResource(const std::string& assetPath) override;
	ENGINE_API std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) override;
	
	void Draw(Shader& shader, const Camera& camera);

private:
	//void loadModel(const std::string& path);
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<std::shared_ptr<Texture>> LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName);


	// Bone data
	std::map<std::string, BoneInfo> mBoneInfoMap; // maps a bone name to its index
	int mBoneCounter = 0;

	// Helper functions for Bones
	auto& GetBoneInfoMap() { return mBoneInfoMap; }
	int& GetBoneCount() { return mBoneCounter; }

	void SetVertexBoneDataToDefault(Vertex& vertex);
	void SetVertexBoneData(Vertex& vertex, int boneID, float weight);
	void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);


};