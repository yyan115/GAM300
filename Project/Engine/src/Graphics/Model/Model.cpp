#include "pch.h"
#include "Graphics/Model/Model.h"
#ifdef __ANDROID__
#include <android/log.h>
#include <android/asset_manager.h>
#include "Platform/AndroidPlatform.h"
#include "Graphics/stb_image.h"
#endif
#include "Graphics/TextureManager.h"
#include <iostream>
#include <unordered_map>
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#include "Logging.hpp"

#ifdef ANDROID
#include <android/log.h>
#endif

Model::Model() {
	// Default constructor - meshes vector is empty by default
}

// Forward declaration for get_file_contents
std::string get_file_contents(const char* filename);

std::string Model::CompileToResource(const std::string& assetPath, bool forAndroid)
{
	Assimp::Importer importer;

//#ifdef __ANDROID__
//	// Set up custom IOSystem for Android AssetManager
//	directory = assetPath.substr(0, assetPath.find_last_of('/'));
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Setting up AndroidIOSystem with base dir: %s", directory.c_str());
//	importer.SetIOHandler(new AndroidIOSystem(directory));
//
//	// On Android, we need to pass just the filename to Assimp since our IOSystem handles the full path
//	std::string filename = assetPath.substr(assetPath.find_last_of('/') + 1);
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Loading OBJ file: %s", filename.c_str());
//	const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs);
//#else
	// The function expects a file path and several post-processing options as its second argument
	// aiProcess_Triangulate tells Assimp that if the model does not (entirely) consist of triangles, it should transform all the model's primitive shapes to triangles first.
	const aiScene* scene = importer.ReadFile(assetPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
//#endif

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
        ENGINE_PRINT("ERROR:ASSIMP:: ", importer.GetErrorString(), "\n");
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Assimp loading failed: %s", importer.GetErrorString());
//#endif
        return std::string{};
	}

	directory = assetPath.substr(0, assetPath.find_last_of('/'));
    // Check metadata
    if (scene->mMetaData) {
        for (unsigned int i = 0; i < scene->mMetaData->mNumProperties; ++i) {
            const aiString* key = &scene->mMetaData->mKeys[i];
            const aiMetadataEntry& entry = scene->mMetaData->mValues[i];

            std::string keyStr = key->C_Str();
            if (keyStr == "SourceAsset_Format") {
				std::string format = static_cast<aiString*>(entry.mData)->C_Str();
                if (format == "Wavefront Object Importer") {
					ENGINE_PRINT("[MODEL] Detected OBJ format from metadata.\n");
					modelFormat = ModelFormat::OBJ;
					flipUVs = true; // OBJ files often need UV flipping
                }
                else if (format == "Autodesk FBX Importer") {
					ENGINE_PRINT("[MODEL] Detected FBX format from metadata.\n");
					modelFormat = ModelFormat::FBX;
					flipUVs = false; // FBX files usually have correct UVs
                }
                else {
                    // Unsupported for now.
					modelFormat = ModelFormat::UNKNOWN;
                }
            }
        }
    }

//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Assimp loaded successfully: %u materials, %u meshes, directory: %s",
//		scene->mNumMaterials, scene->mNumMeshes, directory.c_str());
//
//	// Check if materials have textures
//	for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
//		aiMaterial* mat = scene->mMaterials[i];
//		aiString matName;
//		mat->Get(AI_MATKEY_NAME, matName);
//		unsigned int diffuseCount = mat->GetTextureCount(aiTextureType_DIFFUSE);
//		unsigned int specularCount = mat->GetTextureCount(aiTextureType_SPECULAR);
//		unsigned int normalCount = mat->GetTextureCount(aiTextureType_NORMALS);
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Material %u: %s - diffuse:%u specular:%u normal:%u",
//			i, matName.C_Str(), diffuseCount, specularCount, normalCount);
//
//		if (diffuseCount > 0) {
//			aiString texPath;
//			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
//				__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Material %u diffuse texture path: %s", i, texPath.C_Str());
//			}
//		}
//	}
//
//#endif

    std::filesystem::path p(assetPath);
    modelPath = assetPath;
    modelName = p.stem().generic_string();
	// Recursive function
	ProcessNode(scene->mRootNode, scene);

	return CompileToMesh(assetPath, meshes, forAndroid);
}

void Model::ProcessNode(aiNode* node, const aiScene* scene)
{
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] ProcessNode called - node:%s meshCount:%u childrenCount:%u",
//        node->mName.C_Str(), node->mNumMeshes, node->mNumChildren);
//#endif
	// Process each mesh in this node
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.emplace_back(ProcessMesh(mesh, scene));
	}

	// Process children nodes
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene);
	}

}

Mesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] ProcessMesh called - mesh:%s materialIndex:%u",
//        mesh->mName.C_Str(), mesh->mMaterialIndex);
//#endif
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    //std::vector<std::shared_ptr<Texture>> textures;

    // Process vertices (same as before)
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;

        // Position
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        // Normals
        if (mesh->HasNormals())
        {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        }

        // Texture coordinates
        if (mesh->mTextureCoords[0])
        {
            vertex.texUV.x = mesh->mTextureCoords[0][i].x;
            vertex.texUV.y = mesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.texUV = glm::vec2(0.f, 0.f);
        }

        // Tangents from Assimp
        if (mesh->HasTangentsAndBitangents())
        {
            vertex.tangent.x = mesh->mTangents[i].x;
            vertex.tangent.y = mesh->mTangents[i].y;
            vertex.tangent.z = mesh->mTangents[i].z;
        }
        else
        {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f); // Default tangent
        }

        vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
        vertices.push_back(vertex);
    }

    // Process indices (same as before)
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Create material from Assimp material
    std::shared_ptr<Material> material = nullptr;

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

        // Create new material
        aiString materialName;
        assimpMaterial->Get(AI_MATKEY_NAME, materialName);
        material = std::make_shared<Material>(materialName.C_Str());

        // Load material properties
        aiColor3D color;

        // Ambient
        if (assimpMaterial->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) 
        {
            material->SetAmbient(glm::vec3(color.r, color.g, color.b));
        }

        // Diffuse
        if (assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) 
        {
            material->SetDiffuse(glm::vec3(color.r, color.g, color.b));
        }

        // Specular
        if (assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) 
        {
            material->SetSpecular(glm::vec3(color.r, color.g, color.b));
        }

        // Emissive
        if (assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) 
        {
            material->SetEmissive(glm::vec3(color.r, color.g, color.b));
        }

        // Shininess
        float shininess;
        if (assimpMaterial->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) 
        {
            material->SetShininess(shininess);
        }

        // Opacity
        float opacity;
        if (assimpMaterial->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) 
        {
            material->SetOpacity(opacity);
        }

        // Load textures and assign to material
        LoadMaterialTexture(material, assimpMaterial, aiTextureType_DIFFUSE, "diffuse");
        LoadMaterialTexture(material, assimpMaterial, aiTextureType_SPECULAR, "specular");
        LoadMaterialTexture(material, assimpMaterial, aiTextureType_NORMALS, "normal");

    }

    // If no material was created, use a default one
    if (!material)
    {
        material = Material::CreateDefault();
    }

    // Compile the material for the mesh if it hasn't been compiled before yet.
    std::string materialPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/" + modelName + "_" + material->GetName() + ".mat";
    material->SetName(modelName + "_" + material->GetName());
    AssetManager::GetInstance().CompileUpdatedMaterial(materialPath, material, true);

    Mesh newMesh(vertices, indices, material);
    newMesh.CalculateBoundingBox();
    return newMesh;
}

void Model::LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName) {
    unsigned int textureCount = mat->GetTextureCount(type);
    for (unsigned int i = 0; i < textureCount; i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
		std::filesystem::path texPathObj(str.C_Str());
		texPathObj = texPathObj.stem() / texPathObj.extension(); // Sanitize path

        std::string texturePath = AssetManager::GetInstance().GetAssetPathFromAssetName(texPathObj.generic_string());
		texPathObj = texturePath;
        if (!std::filesystem::exists(texPathObj)) {
            ENGINE_LOG_WARN("[Model] WARNING: Texture file does not exist: ", texturePath, "\n");
            continue;
		}
        // Add a TextureInfo with no texture loaded to the material first.
        // The texture will be loaded when the model is rendered.
        AssetManager::GetInstance().CompileTexture(texturePath, typeName, -1, flipUVs, true);
        std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, nullptr);
        material->SetTexture(static_cast<Material::TextureType>(type), std::move(textureInfo));
    }
	(void)typeName;
}

std::string Model::CompileToMesh(const std::string& modelPathParam, const std::vector<Mesh>& meshesToCompile, bool forAndroid) {
    std::filesystem::path p(modelPathParam);
    std::string meshPath{};
    if (!forAndroid) {
        meshPath = (p.parent_path() / p.stem()).generic_string() + ".mesh";
    }
    else {
        std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
        assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
        meshPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + "_android.mesh";
    }

    // Ensure parent directories exist
    p = meshPath;
    std::filesystem::create_directories(p.parent_path());
    std::ofstream meshFile(meshPath, std::ios::binary);
    if (meshFile.is_open()) {
		// Write the number of meshes to the file as binary data.
		size_t meshCount = meshesToCompile.size();
		meshFile.write(reinterpret_cast<const char*>(&meshCount), sizeof(meshCount));

		// For each mesh, write its data to the file.
        for (const Mesh& mesh : meshesToCompile) {
		    size_t vertexCount = mesh.vertices.size();
            size_t indexCount = mesh.indices.size();

            // Write vertex and index count to the file as binary data.
            meshFile.write(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
		    meshFile.write(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

            // Write vertex data to the file as binary data.
            for (const Vertex& v : mesh.vertices) {
                meshFile.write(reinterpret_cast<const char*>(&v.position), sizeof(v.position));
                meshFile.write(reinterpret_cast<const char*>(&v.normal), sizeof(v.normal));
                meshFile.write(reinterpret_cast<const char*>(&v.color), sizeof(v.color));
                meshFile.write(reinterpret_cast<const char*>(&v.texUV), sizeof(v.texUV));
                meshFile.write(reinterpret_cast<const char*>(&v.tangent), sizeof(v.tangent));
                // meshFile.write(reinterpret_cast<const char*>(&v.tangent), sizeof(v.tangent));
            }

		    // Write index data to the file as binary data.
            meshFile.write(reinterpret_cast<const char*>(mesh.indices.data()), indexCount * sizeof(GLuint));

            // Write material properties to a separate .mat file as binary data.
			size_t nameLength = mesh.material->GetName().size();
			meshFile.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
            std::string meshName = mesh.material->GetName();
            meshFile.write(meshName.data(), nameLength); // Writes actual characters
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetAmbient()), sizeof(mesh.material->GetAmbient()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetDiffuse()), sizeof(mesh.material->GetDiffuse()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetSpecular()), sizeof(mesh.material->GetSpecular()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetEmissive()), sizeof(mesh.material->GetEmissive()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetShininess()), sizeof(mesh.material->GetShininess()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetOpacity()), sizeof(mesh.material->GetOpacity()));

   //         // Write PBR properties to the file as binary data.
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetMetallic()), sizeof(mesh.material->GetMetallic()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetRoughness()), sizeof(mesh.material->GetRoughness()));
		 //   meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetAO()), sizeof(mesh.material->GetAO()));

   //         // Write texture info to the file as binary data.
			//size_t textureCount = mesh.material->GetAllTextureInfo().size();
			//meshFile.write(reinterpret_cast<const char*>(&textureCount), sizeof(textureCount));
   //         auto& allTextureInfo = mesh.material->GetAllTextureInfo();
   //         for (auto it = allTextureInfo.begin(); it != allTextureInfo.end(); ++it) {
   //             // Write texture type
			//	meshFile.write(reinterpret_cast<const char*>(&it->first), sizeof(it->first));
			//	// Write texture path length and path
			//	size_t pathLength = it->second->filePath.size();
			//	meshFile.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
   //             meshFile.write(it->second->filePath.data(), pathLength);
   //         }
        }

		meshFile.close();
        return meshPath;
    }

    return std::string{};
}

bool Model::LoadResource(const std::string& resourcePath, const std::string& assetPath)
{
    meshes.clear();

    // Set model name from asset path
    if (!assetPath.empty()) {
        std::filesystem::path p(assetPath);
        modelName = p.stem().generic_string();
        modelPath = assetPath;
    }
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] LoadResource called with path: %s", assetPath.c_str());
//#endif

    // Use platform abstraction to get asset list (works on Windows, Linux, Android)
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER] ERROR: Platform not available for asset discovery!", "\n");
        return false;
    }

    std::vector<uint8_t> buffer = platform->ReadAsset(resourcePath);
    if (!buffer.empty()) {
        size_t offset = 0;

        // Read the number of meshes from the file.
        size_t meshCount;
        std::memcpy(&meshCount, buffer.data() + offset, sizeof(meshCount));
        offset += sizeof(meshCount);

        // For each mesh, read its data from the file.
        for (size_t i = 0; i < meshCount; ++i) {
            size_t vertexCount, indexCount;
            // Read vertex and index count from the file.
            std::memcpy(&vertexCount, buffer.data() + offset, sizeof(vertexCount));
            offset += sizeof(vertexCount);
            std::memcpy(&indexCount, buffer.data() + offset, sizeof(indexCount));
            offset += sizeof(indexCount);

            // Read vertex data from the file.
            std::vector<Vertex> vertices(vertexCount);
            for (size_t j = 0; j < vertexCount; ++j) {
                Vertex v;
                std::memcpy(&v.position, buffer.data() + offset, sizeof(v.position));
                offset += sizeof(v.position);
                std::memcpy(&v.normal, buffer.data() + offset, sizeof(v.normal));
                offset += sizeof(v.normal);
                std::memcpy(&v.color, buffer.data() + offset, sizeof(v.color));
                offset += sizeof(v.color);
                std::memcpy(&v.texUV, buffer.data() + offset, sizeof(v.texUV));
                offset += sizeof(v.texUV);
                 std::memcpy(&v.tangent, buffer.data() + offset, sizeof(v.tangent));
                 offset += sizeof(v.tangent);
                vertices[j] = std::move(v);
            }

            // Read index data from the file.
            std::vector<GLuint> indices(indexCount);
            std::memcpy(indices.data(), buffer.data() + offset, indexCount * sizeof(GLuint));
            offset += indexCount * sizeof(GLuint);

            // Read material properties from the file.
            //std::shared_ptr<Material> material = std::make_shared<Material>();
            // Name
            size_t nameLength;
            std::memcpy(&nameLength, buffer.data() + offset, sizeof(nameLength));
            offset += sizeof(nameLength);
            std::string matName(nameLength, '\0'); // Pre-size the string
            std::memcpy(&matName[0], buffer.data() + offset, nameLength);
            offset += nameLength;
            std::string materialPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/" + matName + ".mat";
            // Load the material
            auto material = ResourceManager::GetInstance().GetResource<Material>(materialPath);
//            material->SetName(meshName);
//            // Ambient
//            glm::vec3 ambient;
//            std::memcpy(&ambient, buffer.data() + offset, sizeof(ambient));
//            offset += sizeof(ambient);
//            material->SetAmbient(ambient);
//            // Diffuse
//            glm::vec3 diffuse;
//            std::memcpy(&diffuse, buffer.data() + offset, sizeof(diffuse));
//            offset += sizeof(diffuse);
//            material->SetDiffuse(diffuse);
//            // Specular
//            glm::vec3 specular;
//            std::memcpy(&specular, buffer.data() + offset, sizeof(specular));
//            offset += sizeof(specular);
//            material->SetSpecular(specular);
//            // Emissive
//            glm::vec3 emissive;
//            std::memcpy(&emissive, buffer.data() + offset, sizeof(emissive));
//            offset += sizeof(emissive);
//            material->SetEmissive(emissive);
//            // Shininess
//            float shininess;
//            std::memcpy(&shininess, buffer.data() + offset, sizeof(shininess));
//            offset += sizeof(shininess);
//            material->SetShininess(shininess);
//            // Opacity
//            float opacity;
//            std::memcpy(&opacity, buffer.data() + offset, sizeof(opacity));
//            offset += sizeof(opacity);
//            material->SetOpacity(opacity);
//            // Metallic
//            float metallic;
//            std::memcpy(&metallic, buffer.data() + offset, sizeof(metallic));
//            offset += sizeof(metallic);
//            material->SetMetallic(metallic);
//            // Roughness
//            float roughness;
//            std::memcpy(&roughness, buffer.data() + offset, sizeof(roughness));
//            offset += sizeof(roughness);
//            material->SetRoughness(roughness);
//            // AO
//            float ao;
//            std::memcpy(&ao, buffer.data() + offset, sizeof(ao));
//            offset += sizeof(ao);
//            material->SetAO(ao);
//
//            // Read texture paths from the file.
//            size_t textureCount;
//            std::vector<std::shared_ptr<Texture>> textures;
//            std::memcpy(&textureCount, buffer.data() + offset, sizeof(textureCount));
//            offset += sizeof(textureCount);
//            for (size_t j = 0; j < textureCount; ++j) {
//                Material::TextureType texType;
//                std::memcpy(&texType, buffer.data() + offset, sizeof(texType));
//                offset += sizeof(texType);
//                size_t pathLength;
//                std::memcpy(&pathLength, buffer.data() + offset, sizeof(pathLength));
//                offset += sizeof(pathLength);
//                std::string texturePath(buffer.data() + offset, buffer.data() + offset + pathLength);
//                // strip trailing nulls
//                texturePath.erase(std::find(texturePath.begin(), texturePath.end(), '\0'), texturePath.end());
//                offset += pathLength;
//
//                // Load texture via Resource Manager
//#ifndef ANDROID
//                std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
//#else
//                texturePath = texturePath.substr(texturePath.find("Resources"));
//                std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
//#endif
//                if (texture) {
//                    std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
//                    material->SetTexture(texType, std::move(textureInfo));
//
//                    // Assign the texture type
//                    switch (texType) {
//                    case Material::TextureType::DIFFUSE:
//                        texture->GetType() = "diffuse";
//                        break;
//                    case Material::TextureType::SPECULAR:
//                        texture->GetType() = "specular";
//                        break;
//                    case Material::TextureType::NORMAL:
//                        texture->GetType() = "normal";
//                        break;
//                    case Material::TextureType::EMISSIVE:
//                        texture->GetType() = "emissive";
//                        break;
//                        // Add other cases as needed
//                    default:
//                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MODEL] Warning: Unhandled texture type in model loading.\n");
//                        texture->GetType() = "unknown";
//                        break;
//                    }
//                }
//
//                textures.push_back(texture);
//            }

        Mesh newMesh(vertices, indices, textures, material);
        newMesh.CalculateBoundingBox();
        meshes.push_back(std::move(newMesh));            

        CalculateBoundingBox();

        return true;
    }

    return false;
}

bool Model::ReloadResource(const std::string& resourcePath, const std::string& assetPath)
{
    return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Model::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid)
{
    assetPath, currentMetaData, forAndroid;
    return std::shared_ptr<AssetMeta>();
}

void Model::Draw(Shader& shader, const Camera& camera)
{
#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Starting Model::Draw - meshes.size=%zu, shader.ID=%u", meshes.size(), shader.ID);

	// Ensure OpenGL context is current for Android
	auto platform = WindowManager::GetPlatform();
	if (platform) {
		if (!platform->MakeContextCurrent()) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Failed to make OpenGL context current for model drawing");
			return;
		}
		/*__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] OpenGL context made current for model drawing");*/
	}

	// Validate shader
	if (shader.ID == 0 || !glIsProgram(shader.ID)) {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Invalid shader program ID: %u", shader.ID);
		return;
	}

	// Check if meshes vector is empty
	if (meshes.empty()) {
		__android_log_print(ANDROID_LOG_WARN, "GAM300", "[MODEL] No meshes to draw");
		return;
	}
#endif

	for (size_t i = 0; i < meshes.size(); ++i)
	{
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Drawing mesh %zu/%zu - vertices=%zu, indices=%zu", i+1, meshes.size(), meshes[i].vertices.size(), meshes[i].indices.size());

		// Validate mesh before drawing
		if (meshes[i].vertices.empty()) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Mesh %zu has no vertices, skipping", i+1);
			continue;
		}
		if (meshes[i].indices.empty()) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Mesh %zu has no indices, skipping", i+1);
			continue;
		}
#endif

		meshes[i].Draw(shader, camera);

#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Successfully drew mesh %zu/%zu", i+1, meshes.size());
#endif
	}

#ifdef ANDROID
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Model::Draw completed successfully");
#endif
}

void Model::Draw(Shader& shader, const Camera& camera, std::shared_ptr<Material> entityMaterial)
{
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Starting Model::Draw with entity material - meshes.size=%zu, shader.ID=%u", meshes.size(), shader.ID);
//#endif

	for (size_t i = 0; i < meshes.size(); ++i)
	{
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Drawing mesh %zu/%zu with entity material", i+1, meshes.size());
//#endif
		// Use entity material if available, otherwise use mesh default
		std::shared_ptr<Material> meshMaterial = entityMaterial ? entityMaterial : meshes[i].material;
		if (meshMaterial && meshMaterial != meshes[i].material) {
			// Temporarily override the mesh material for this draw call
			std::shared_ptr<Material> originalMaterial = meshes[i].material;
			meshes[i].material = meshMaterial;
			meshes[i].Draw(shader, camera);
			meshes[i].material = originalMaterial; // Restore original
		} else {
			meshes[i].Draw(shader, camera);
		}

//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Successfully drew mesh %zu/%zu with entity material", i+1, meshes.size());
//#endif
	}

//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Model::Draw with entity material completed successfully");
//#endif
}

#ifdef __ANDROID__
// Forward declaration
std::string get_file_contents(const char* filename);

// AndroidIOStream implementation
AndroidIOStream::AndroidIOStream(const std::string& path, const std::string& content)
    : m_path(path), m_stream(content) {
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOStream] Created stream for: %s (%d bytes)", path.c_str(), (int)content.size());
}

AndroidIOStream::~AndroidIOStream() {
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOStream] Destroyed stream for: %s", m_path.c_str());
}

size_t AndroidIOStream::Read(void* pvBuffer, size_t pSize, size_t pCount) {
    size_t totalBytes = pSize * pCount;
    m_stream.read(static_cast<char*>(pvBuffer), totalBytes);
    size_t bytesRead = m_stream.gcount();
    return bytesRead / pSize; // Return number of elements read
}

size_t AndroidIOStream::Write(const void* pvBuffer, size_t pSize, size_t pCount) {
    // Read-only implementation
    return 0;
}

aiReturn AndroidIOStream::Seek(size_t pOffset, aiOrigin pOrigin) {
    std::ios::seekdir dir;
    switch (pOrigin) {
        case aiOrigin_SET: dir = std::ios::beg; break;
        case aiOrigin_CUR: dir = std::ios::cur; break;
        case aiOrigin_END: dir = std::ios::end; break;
        default: return AI_FAILURE;
    }

    m_stream.seekg(pOffset, dir);
    return m_stream.good() ? AI_SUCCESS : AI_FAILURE;
}

size_t AndroidIOStream::Tell() const {
    return const_cast<std::stringstream&>(m_stream).tellg();
}

size_t AndroidIOStream::FileSize() const {
    auto& stream = const_cast<std::stringstream&>(m_stream);
    auto currentPos = stream.tellg();
    stream.seekg(0, std::ios::end);
    size_t size = stream.tellg();
    stream.seekg(currentPos);
    return size;
}

void AndroidIOStream::Flush() {
    // Nothing to flush for read-only stream
}

// AndroidIOSystem implementation
AndroidIOSystem::AndroidIOSystem(const std::string& baseDir) : m_baseDir(baseDir) {
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOSystem] Created with base dir: %s", baseDir.c_str());
}

AndroidIOSystem::~AndroidIOSystem() {
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOSystem] Destroyed");
}

bool AndroidIOSystem::Exists(const char* pFile) const {
    std::string fullPath = m_baseDir + "/" + std::string(pFile);
    std::string content = get_file_contents(fullPath.c_str());
    bool exists = !content.empty();
   // __android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOSystem] Exists check for: %s -> %s", fullPath.c_str(), exists ? "true" : "false");
    return exists;
}

char AndroidIOSystem::getOsSeparator() const {
    return '/';
}

Assimp::IOStream* AndroidIOSystem::Open(const char* pFile, const char* pMode) {
    std::string fullPath = m_baseDir + "/" + std::string(pFile);
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOSystem] Opening file: %s (mode: %s)", fullPath.c_str(), pMode);

    std::string content = get_file_contents(fullPath.c_str());
    if (content.empty()) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[AndroidIOSystem] Failed to load file: %s", fullPath.c_str());
        return nullptr;
    }

    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOSystem] Successfully loaded file: %s (%d bytes)", fullPath.c_str(), (int)content.size());
    return new AndroidIOStream(fullPath, content);
}

void AndroidIOSystem::Close(Assimp::IOStream* pFile) {
    //__android_log_print(ANDROID_LOG_INFO, "GAM300", "[AndroidIOSystem] Closing stream");
    delete pFile;
}
#endif