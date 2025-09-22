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

class ENGINE_API Model : public IAsset {
public:
	std::vector<Mesh> meshes;
	std::string directory;

	//Model(const std::string& filePath);
	bool LoadAsset(const std::string& path) override;
	void Draw(Shader& shader, const Camera& camera);

private:
	//void loadModel(const std::string& path);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<std::shared_ptr<Texture>> loadMaterialTexture(aiMaterial* mat, aiTextureType type, std::string typeName);

};