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

};