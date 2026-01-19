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
#include "Math/Matrix4x4.hpp"
#include "Graphics/Model/BoneInfo.hpp"

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
class Animator;
class ModelRenderComponent;

struct ModelNode {
    std::string name;
    Matrix4x4 localTransform; // Transform relative to parent
	std::vector<ModelNode> children;
};

class ENGINE_API Model : public IAsset {
public:
	std::vector<Mesh> meshes;
	std::string directory;
    std::string modelName;
    std::string modelPath;
    std::shared_ptr<ModelMeta> metaData;

    // Model node hierarchy
    ModelNode rootNode;

    // Bone data
    std::map<std::string, BoneInfo> mBoneInfoMap; // maps a bone name to its index
    int mBoneCounter = 0;

	Model();
    Model(const Model& other) = default;
    Model(std::shared_ptr<AssetMeta> modelMeta);
	virtual ~Model() = default;
	//Model(const std::string& filePath);

    static float GetMaxExtent(const aiScene* scene);
    static float CalculateAutoScale(const aiScene* scene);

    std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	std::string CompileToMesh(const std::string& modelPath, std::vector<Mesh>& meshesToCompile, bool forAndroid = false);
	bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;
	
	void Draw(Shader& shader, const Camera& camera, const ModelRenderComponent* modelComp = nullptr);
	void Draw(Shader& shader, const Camera& camera, std::shared_ptr<Material> entityMaterial, const ModelRenderComponent& modelComp);
    void Draw(Shader& shader, const Camera& camera, std::shared_ptr<Material> entityMaterial, const ModelRenderComponent& modelComp, const Animator* animator);

	// Helper functions for Bones
	auto& GetBoneInfoMap() { return mBoneInfoMap;}
    int& GetBoneCount() { return mBoneCounter; }

	void SetVertexBoneDataToDefault(Vertex& vertex);
	void SetVertexBoneData(Vertex& vertex, int boneID, float weight);
    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);

    AABB GetBoundingBox() const { return modelBoundingBox; }

    void CalculateBoundingBox() 
    {
        if (meshes.empty()) 
        {
            modelBoundingBox = AABB(glm::vec3(0.0f), glm::vec3(0.0f));
            return;
        }

        glm::vec3 min(FLT_MAX);
        glm::vec3 max(-FLT_MAX);

        // Combine all mesh bounding boxes
        for (const auto& mesh : meshes) 
        {
            AABB meshBox = mesh.GetBoundingBox();
            min = glm::min(min, meshBox.min);
            max = glm::max(max, meshBox.max);
        }

        modelBoundingBox = AABB(min, max);
    }

private:
    enum class ModelFormat {
        UNKNOWN,
        OBJ,
        FBX,
        GLTF,
        GLB,
        DAE,
        STL,
		PLY
    } modelFormat{};
	bool flipUVs = false;

	//void loadModel(const std::string& path);
	void ProcessNode(aiNode* node, ModelNode& dest, const aiScene* scene);
	Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);

	void WriteModelNode(std::ofstream& meshFile, const ModelNode& node);
	void ReadModelNode(std::vector<unsigned char>& buffer, size_t& offset, ModelNode& node);

    void LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName);
    AABB modelBoundingBox;
};