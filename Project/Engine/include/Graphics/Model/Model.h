#pragma once
#include "Graphics/Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <string>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include "Asset Manager/Asset.hpp"
#include "../../Engine.h"

class Material;

#ifdef __ANDROID__
// Custom IOStream for Android AssetManager
class AndroidIOStream : public Assimp::IOStream {
private:
    std::stringstream m_stream;
    std::string m_path;

public:
    AndroidIOStream(const std::string& path, const std::string& content);
    ~AndroidIOStream();

    size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override;
    size_t Write(const void* pvBuffer, size_t pSize, size_t pCount) override;
    aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override;
    size_t Tell() const override;
    size_t FileSize() const override;
    void Flush() override;
};

// Custom IOSystem for Android AssetManager
class AndroidIOSystem : public Assimp::IOSystem {
private:
    std::string m_baseDir;

public:
    AndroidIOSystem(const std::string& baseDir);
    ~AndroidIOSystem();

    bool Exists(const char* pFile) const override;
    char getOsSeparator() const override;
    Assimp::IOStream* Open(const char* pFile, const char* pMode = "rb") override;
    void Close(Assimp::IOStream* pFile) override;
};
#endif

class ENGINE_API Model : public IAsset {
public:
	std::vector<Mesh> meshes;
	std::string directory;

	Model();
	virtual ~Model() = default;
	//Model(const std::string& filePath);
    std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	std::string CompileToMesh(const std::string& modelPath, const std::vector<Mesh>& meshesToCompile, bool forAndroid = false);
	bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;
	
	void Draw(Shader& shader, const Camera& camera);
	void Draw(Shader& shader, const Camera& camera, std::shared_ptr<Material> entityMaterial);

private:
	//void loadModel(const std::string& path);
	void ProcessNode(aiNode* node, const aiScene* scene);
	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
	//std::vector<std::shared_ptr<Texture>> LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName);
    void LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName);
};