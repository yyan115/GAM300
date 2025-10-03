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
	const aiScene* scene = importer.ReadFile(assetPath, aiProcess_Triangulate | aiProcess_FlipUVs);
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

        SetVertexBoneDataToDefault(vertex);

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
//#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Processing mesh material - mMaterialIndex=%d, total materials in scene=%d", mesh->mMaterialIndex, scene->mNumMaterials);
//#endif
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

//#ifdef ANDROID
//        // Debug material properties to see what Assimp actually loaded
//        aiString matFile;
//        if (assimpMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), matFile) == AI_SUCCESS) {
//            __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Material has diffuse texture: %s", matFile.C_Str());
//        } else {
//            __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Material has no diffuse texture in Assimp data");
//        }
//
//        unsigned int textureCount = assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE);
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Assimp reports %u diffuse textures for this material", textureCount);
//#endif

        // Create new material
        aiString materialName;
        assimpMaterial->Get(AI_MATKEY_NAME, materialName);
        material = std::make_shared<Material>(materialName.C_Str());
//#ifdef ANDROID
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Created material from Assimp: %s, pointer=%p", materialName.C_Str(), material.get());
//#endif

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

        // Diffuse textures
//#ifdef __ANDROID__
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] About to call LoadMaterialTexture for diffuse textures");
//#endif
        //std::vector<std::shared_ptr<Texture>> diffuseMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_DIFFUSE, "diffuse");
//#ifdef __ANDROID__
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] LoadMaterialTexture returned %zu diffuse textures", diffuseMaps.size());
//#endif
        //if (!diffuseMaps.empty()) {
        //    material->SetTexture(TextureType::DIFFUSE, diffuseMaps[0]);
        //}

        // Specular textures
        //std::vector<std::shared_ptr<Texture>> specularMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_SPECULAR, "specular");
        //if (!specularMaps.empty()) 
        //{
        //    material->SetTexture(TextureType::SPECULAR, specularMaps[0]);
        //}

        // Normal maps
        //std::vector<std::shared_ptr<Texture>> normalMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_NORMALS, "normal");
        //if (!normalMaps.empty()) 
        //{
        //    material->SetTexture(TextureType::NORMAL, normalMaps[0]);
        //}

        // Height maps
        //std::vector<std::shared_ptr<Texture>> heightMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_HEIGHT, "height");
        //if (!heightMaps.empty()) {
        //    material->SetTexture(TextureType::HEIGHT, heightMaps[0]);
        //}

        // Keep old texture list for backward compatibility
        //textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        //textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    }

    // If no material was created, use a default one
    if (!material)
    {
//#ifdef ANDROID
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] No material found, creating default material");
//#endif
        material = Material::CreateDefault();
//#ifdef ANDROID
//        __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Created default material, pointer=%p", material.get());
//#endif
    }

	// Extract bone weights for vertices
	ExtractBoneWeightForVertices(vertices, mesh, scene);

//#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Creating mesh with material pointer=%p", material.get());
//#endif

    // Compile the material for the mesh if it hasn't been compiled before yet.
    std::string materialPath = modelName + "_" + material->GetName() + ".mat";
    AssetManager::GetInstance().CompileUpdatedMaterial(materialPath, material);
    return Mesh(vertices, indices, material);
}

//std::vector<std::shared_ptr<Texture>> Model::LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName)
//{
////#ifdef __ANDROID__
////	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] LoadMaterialTexture ENTRY - type:%d typeName:%s", (int)type, typeName.c_str());
////#endif
//	typeName;
//	std::vector<std::shared_ptr<Texture>> textures;
//	//TextureManager& textureManager = TextureManager::getInstance();
//
////#ifdef __ANDROID__
////	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] About to call mat->GetTextureCount(type=%d)", (int)type);
////#endif
//	unsigned int textureCount = mat->GetTextureCount(type);
////#ifdef __ANDROID__
////	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] GetTextureCount returned: %u", textureCount);
////#endif
////#ifdef __ANDROID__
////	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] LoadMaterialTexture called - type:%d typeName:%s count:%u",
////		(int)type, typeName.c_str(), textureCount);
////#endif
//
////#ifdef __ANDROID__
////	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] About to enter texture loading loop with textureCount=%u", textureCount);
////#endif
//	for (unsigned int i = 0; i < textureCount; i++)
//	{
////#ifdef __ANDROID__
////		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Loop iteration %u: getting texture info", i);
////#endif
//		aiString str;
//		mat->GetTexture(type, i, &str);
////#ifdef __ANDROID__
////		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] GetTexture returned: %s", str.C_Str());
////#endif
//		std::string texturePath = directory + '/' + str.C_Str();
//
////#ifdef __ANDROID__
////		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Attempting to load texture: %s", texturePath.c_str());
////#endif
//
//		// Use the asset manager
//		std::shared_ptr<Texture> texture = nullptr;
//
//		// Check if we already have this texture loaded (texture sharing to save memory)
//		//static std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache;
//		//auto cacheIt = textureCache.find(texturePath);
//		//if (cacheIt != textureCache.end()) {
//		//	texture = cacheIt->second;
////#ifdef __ANDROID__
////			__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Using cached texture: %s", texturePath.c_str());
////#endif
//		//} else {
//
////#ifdef __ANDROID__
////		// On Android, create texture directly from JPG data since we can't write DDS files
////		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Creating texture directly from Android assets: %s", texturePath.c_str());
////		texture = std::make_shared<Texture>(typeName.c_str(), -1);
////
////		// Load texture data directly from Android assets
////		auto* platform = WindowManager::GetPlatform();
////		if (platform) {
////			AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
////			AAssetManager* assetManager = androidPlatform->GetAssetManager();
////
////			if (assetManager) {
////				AAsset* asset = AAssetManager_open(assetManager, texturePath.c_str(), AASSET_MODE_BUFFER);
////				if (asset) {
////					off_t assetLength = AAsset_getLength(asset);
////					const unsigned char* assetData = (const unsigned char*)AAsset_getBuffer(asset);
////
////					if (assetData && assetLength > 0) {
////						int widthImg, heightImg, numColCh;
////						stbi_set_flip_vertically_on_load(true);
////						unsigned char* bytes = stbi_load_from_memory(assetData, (int)assetLength, &widthImg, &heightImg, &numColCh, 0);
////
////						if (bytes) {
////							// Generate OpenGL texture
////							glGenTextures(1, &texture->ID);
////							glBindTexture(GL_TEXTURE_2D, texture->ID);
////							texture->target = GL_TEXTURE_2D;
////
////							// Configure texture parameters
////							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
////							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
////							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
////							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
////
////							// Upload texture data
////							GLenum format = (numColCh == 4) ? GL_RGBA : GL_RGB;
////							glTexImage2D(GL_TEXTURE_2D, 0, format, widthImg, heightImg, 0, format, GL_UNSIGNED_BYTE, bytes);
////							glGenerateMipmap(GL_TEXTURE_2D);
////
////							// Cleanup
////							stbi_image_free(bytes);
////							glBindTexture(GL_TEXTURE_2D, 0);
////
////							__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Successfully created texture from Android assets: %s (%dx%d, %d channels)", texturePath.c_str(), widthImg, heightImg, numColCh);
////						} else {
////							__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Failed to decode texture from memory: %s", texturePath.c_str());
////							texture.reset();
////						}
////					}
////					AAsset_close(asset);
////				} else {
////					__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Failed to open texture asset: %s", texturePath.c_str());
////					texture.reset();
////				}
////			}
////		}
////#else
//        //std::cout << "[MODEL] DEBUG: Attempting to compile texture: " << texturePath << std::endl;
//        if (AssetManager::GetInstance().CompileTexture(texturePath, typeName, -1)) {
//			// Use the original texture path - ResourceManager will handle finding the compiled DDS
//			//std::cout << "[MODEL] DEBUG: Compiled texture successfully, loading with original path: " << texturePath << std::endl;
//#ifndef ANDROID
//		    texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
//#else
//            texturePath = texturePath.substr(texturePath.find("Resources"));
//            texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
//#endif
//		    //std::cout << "[MODEL] DEBUG: Loaded texture resource, valid: " << (texture != nullptr) << std::endl;
//		    //if (texture) {
//		        //std::cout << "[MODEL] DEBUG: Texture ID: " << texture->ID << ", type: " << texture->type << std::endl;
//		    //}
//
//		}
////#endif
//
//			//// Cache the newly created texture
//			//if (texture) {
//			//	textureCache[texturePath] = texture;
//			//}
//		//}
//
//		// Common code for both platforms after texture processing
//		//if (texture) {
//		textures.push_back(texture);
//		std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
//		material->SetTexture(static_cast<Material::TextureType>(type), std::move(textureInfo));
//
//			//std::cout << "[MODEL] DEBUG: Texture set successfully on material, type: " << (int)type << ", path: " << texturePath << std::endl;
////#ifdef __ANDROID__
////			__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Texture set successfully: %s", texturePath.c_str());
////#endif
//		//} else {
//			//std::cout << "[MODEL] DEBUG: Failed to get texture resource: " << texturePath << std::endl;
////#ifdef __ANDROID__
////			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Failed to get texture resource: %s", texturePath.c_str());
////#endif
//		//}
//	}
//
//	return textures;
//}

void Model::LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName) {
    unsigned int textureCount = mat->GetTextureCount(type);
    for (unsigned int i = 0; i < textureCount; i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        std::string texturePath = directory + '/' + str.C_Str();
        // Add a TextureInfo with no texture loaded to the material first.
        // The texture will be loaded when the model is rendered.
        std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, nullptr);
        material->SetTexture(static_cast<Material::TextureType>(type), std::move(textureInfo));
    }
}

std::string Model::CompileToMesh(const std::string& modelPath, const std::vector<Mesh>& meshesToCompile, bool forAndroid) {
    std::filesystem::path p(modelPath);
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
            }

		    // Write index data to the file as binary data.
            meshFile.write(reinterpret_cast<const char*>(mesh.indices.data()), indexCount * sizeof(GLuint));

            // Write material properties to a separate .mat file as binary data.
			size_t nameLength = mesh.material->GetName().size();
			meshFile.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
            std::string meshName = mesh.material->GetName();
            meshFile.write(meshName.data(), nameLength); // Writes actual characters
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetAmbient()), sizeof(mesh.material->GetAmbient()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetDiffuse()), sizeof(mesh.material->GetDiffuse()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetSpecular()), sizeof(mesh.material->GetSpecular()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetEmissive()), sizeof(mesh.material->GetEmissive()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetShininess()), sizeof(mesh.material->GetShininess()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetOpacity()), sizeof(mesh.material->GetOpacity()));

            // Write PBR properties to the file as binary data.
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetMetallic()), sizeof(mesh.material->GetMetallic()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetRoughness()), sizeof(mesh.material->GetRoughness()));
		    meshFile.write(reinterpret_cast<const char*>(&mesh.material->GetAO()), sizeof(mesh.material->GetAO()));

            // Write texture info to the file as binary data.
			size_t textureCount = mesh.material->GetAllTextureInfo().size();
			meshFile.write(reinterpret_cast<const char*>(&textureCount), sizeof(textureCount));
            auto& allTextureInfo = mesh.material->GetAllTextureInfo();
            for (auto it = allTextureInfo.begin(); it != allTextureInfo.end(); ++it) {
                // Write texture type
				meshFile.write(reinterpret_cast<const char*>(&it->first), sizeof(it->first));
				// Write texture path length and path
				size_t pathLength = it->second->filePath.size();
				meshFile.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
                meshFile.write(it->second->filePath.data(), pathLength);
            }
        }

		meshFile.close();

        //if (!forAndroid) {
        //    // Save the mesh file to the root project Resources folder as well.
        //    try {
        //        std::filesystem::copy_file(meshPath, (FileUtilities::GetSolutionRootDir() / meshPath).generic_string(),
        //            std::filesystem::copy_options::overwrite_existing);
        //    }
        //    catch (const std::filesystem::filesystem_error& e) {
        //        std::cerr << "[MODEL] Copy failed: " << e.what() << std::endl;
        //    }
        //}

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
        modelName = p.filename().generic_string();
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
                vertices[j] = std::move(v);
            }

            // Read index data from the file.
            std::vector<GLuint> indices(indexCount);
            std::memcpy(indices.data(), buffer.data() + offset, indexCount * sizeof(GLuint));
            offset += indexCount * sizeof(GLuint);

            // Read material properties from the file.
            std::shared_ptr<Material> material = std::make_shared<Material>();
            // Name
            size_t nameLength;
            std::memcpy(&nameLength, buffer.data() + offset, sizeof(nameLength));
            offset += sizeof(nameLength);
            std::string meshName(nameLength, '\0'); // Pre-size the string
            std::memcpy(&meshName[0], buffer.data() + offset, nameLength);
            offset += nameLength;
            material->SetName(meshName);
            // Ambient
            glm::vec3 ambient;
            std::memcpy(&ambient, buffer.data() + offset, sizeof(ambient));
            offset += sizeof(ambient);
            material->SetAmbient(ambient);
            // Diffuse
            glm::vec3 diffuse;
            std::memcpy(&diffuse, buffer.data() + offset, sizeof(diffuse));
            offset += sizeof(diffuse);
            material->SetDiffuse(diffuse);
            // Specular
            glm::vec3 specular;
            std::memcpy(&specular, buffer.data() + offset, sizeof(specular));
            offset += sizeof(specular);
            material->SetSpecular(specular);
            // Emissive
            glm::vec3 emissive;
            std::memcpy(&emissive, buffer.data() + offset, sizeof(emissive));
            offset += sizeof(emissive);
            material->SetEmissive(emissive);
            // Shininess
            float shininess;
            std::memcpy(&shininess, buffer.data() + offset, sizeof(shininess));
            offset += sizeof(shininess);
            material->SetShininess(shininess);
            // Opacity
            float opacity;
            std::memcpy(&opacity, buffer.data() + offset, sizeof(opacity));
            offset += sizeof(opacity);
            material->SetOpacity(opacity);
            // Metallic
            float metallic;
            std::memcpy(&metallic, buffer.data() + offset, sizeof(metallic));
            offset += sizeof(metallic);
            material->SetMetallic(metallic);
            // Roughness
            float roughness;
            std::memcpy(&roughness, buffer.data() + offset, sizeof(roughness));
            offset += sizeof(roughness);
            material->SetRoughness(roughness);
            // AO
            float ao;
            std::memcpy(&ao, buffer.data() + offset, sizeof(ao));
            offset += sizeof(ao);
            material->SetAO(ao);

            // Read texture paths from the file.
            size_t textureCount;
            std::vector<std::shared_ptr<Texture>> textures;
            std::memcpy(&textureCount, buffer.data() + offset, sizeof(textureCount));
            offset += sizeof(textureCount);
            for (size_t j = 0; j < textureCount; ++j) {
                Material::TextureType texType;
                std::memcpy(&texType, buffer.data() + offset, sizeof(texType));
                offset += sizeof(texType);
                size_t pathLength;
                std::memcpy(&pathLength, buffer.data() + offset, sizeof(pathLength));
                offset += sizeof(pathLength);
                std::string texturePath(buffer.data() + offset, buffer.data() + offset + pathLength);
                // strip trailing nulls
                texturePath.erase(std::find(texturePath.begin(), texturePath.end(), '\0'), texturePath.end());
                offset += pathLength;

                // Load texture via Resource Manager
#ifndef ANDROID
                std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
#else
                texturePath = texturePath.substr(texturePath.find("Resources"));
                std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
#endif
                if (texture) {
                    std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
                    material->SetTexture(texType, std::move(textureInfo));

                    // Assign the texture type
                    switch (texType) {
                    case Material::TextureType::DIFFUSE:
                        texture->type = "diffuse";
                        break;
                    case Material::TextureType::SPECULAR:
                        texture->type = "specular";
                        break;
                    case Material::TextureType::NORMAL:
                        texture->type = "normal";
                        break;
                    case Material::TextureType::EMISSIVE:
                        texture->type = "emissive";
                        break;
                        // Add other cases as needed
                    default:
                        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MODEL] Warning: Unhandled texture type in model loading.\n");
                        texture->type = "unknown";
                        break;
                    }
                }

                textures.push_back(texture);
            }

            meshes.emplace_back(vertices, indices, textures, material);
        }

        return true;
    }
//	std::ifstream meshFile(resourcePath, std::ios::binary);
//
////#ifdef __ANDROID__
////    // On Android, always force OBJ loading to ensure textures are processed
////    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Forcing OBJ processing on Android for textures");
////    meshFile.close();
////#endif
//
//    if (meshFile.is_open() && !meshFile.fail()) {
//		// Read the number of meshes from the file.
//        size_t meshCount;
//		meshFile.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
//
//        // For each mesh, read its data from the file.
//        for (size_t i = 0; i < meshCount; ++i) {
//            size_t vertexCount, indexCount;
//            // Read vertex and index count from the file.
//            meshFile.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
//			meshFile.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
//
//            // Read vertex data from the file.
//			std::vector<Vertex> vertices(vertexCount);
//            for (size_t j = 0; j < vertexCount; ++j) {
//                Vertex v;
//                meshFile.read(reinterpret_cast<char*>(&v.position), sizeof(v.position));
//				meshFile.read(reinterpret_cast<char*>(&v.normal), sizeof(v.normal));
//				meshFile.read(reinterpret_cast<char*>(&v.color), sizeof(v.color));
//                meshFile.read(reinterpret_cast<char*>(&v.texUV), sizeof(v.texUV));
//				vertices[j] = std::move(v);
//            }
//
//			// Read index data from the file.
//            std::vector<GLuint> indices(indexCount);
//			meshFile.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(GLuint));
//
//            // Read material properties from the file.
//			std::shared_ptr<Material> material = std::make_shared<Material>();
//            // Name
//			size_t nameLength;
//			meshFile.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
//            std::string meshName(nameLength, '\0'); // Pre-size the string
//			meshFile.read(reinterpret_cast<char*>(&meshName[0]), nameLength);
//			material->SetName(meshName);
//            // Ambient
//            glm::vec3 ambient;
//			meshFile.read(reinterpret_cast<char*>(&ambient), sizeof(ambient));
//			material->SetAmbient(ambient);
//            // Diffuse
//			glm::vec3 diffuse;
//			meshFile.read(reinterpret_cast<char*>(&diffuse), sizeof(diffuse));
//			material->SetDiffuse(diffuse);
//			// Specular
//			glm::vec3 specular;
//			meshFile.read(reinterpret_cast<char*>(&specular), sizeof(specular));
//			material->SetSpecular(specular);
//            // Emissive
//            glm::vec3 emissive;
//			meshFile.read(reinterpret_cast<char*>(&emissive), sizeof(emissive));
//            material->SetEmissive(emissive);
//            // Shininess
//			float shininess;
//			meshFile.read(reinterpret_cast<char*>(&shininess), sizeof(shininess));
//            material->SetShininess(shininess);
//			// Opacity
//			float opacity;
//            meshFile.read(reinterpret_cast<char*>(&opacity), sizeof(opacity));
//			material->SetOpacity(opacity);
//            // Metallic
//			float metallic;
//			meshFile.read(reinterpret_cast<char*>(&metallic), sizeof(metallic));
//            material->SetMetallic(metallic);
//			// Roughness
//			float roughness;
//            meshFile.read(reinterpret_cast<char*>(&roughness), sizeof(roughness));
//			material->SetRoughness(roughness);
//			// AO
//            float ao;
//			meshFile.read(reinterpret_cast<char*>(&ao), sizeof(ao));
//			material->SetAO(ao);
//
//			// Read texture paths from the file.
//			size_t textureCount;
//            std::vector<std::shared_ptr<Texture>> textures;
//            meshFile.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));
//            for (size_t j = 0; j < textureCount; ++j) {
//                Material::TextureType texType;
//				meshFile.read(reinterpret_cast<char*>(&texType), sizeof(texType));
//				size_t pathLength;
//				meshFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
//				std::string texturePath(pathLength, '\0');
//				meshFile.read(reinterpret_cast<char*>(&texturePath[0]), pathLength);
//
//                // Load texture via Resource Manager
//				std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
//                if (texture) {
//                    std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
//                    material->SetTexture(texType, std::move(textureInfo));
//
//                    // Assign the texture type
//                    switch (texType) {
//                        case Material::TextureType::DIFFUSE:
//                            texture->type = "diffuse";
//							break;
//                        case Material::TextureType::SPECULAR:
//							texture->type = "specular";
//                            break;
//						case Material::TextureType::NORMAL:
//                            texture->type = "normal";
//                            break;
//						case Material::TextureType::EMISSIVE:
//                            texture->type = "emissive";
//                            break;
//                        // Add other cases as needed
//                        default:
//							std::cerr << "[MODEL] Warning: Unhandled texture type in model loading.\n";
//                            texture->type = "unknown";
//							break;
//                    }
//				}
//
//                textures.push_back(texture);
//            }
//
//            meshes.emplace_back(vertices, indices, textures, material);
//        }
//
//        return true;
//    }

//    // Fallback: If .mesh file doesn't exist, try to load from original .obj file
//#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_WARN, "GAM300", "[MODEL] .mesh file not found: %s, attempting to load from .obj", resourcePath.c_str());
//#endif
//    std::cerr << "[MODEL] .mesh file not found: " << resourcePath << ", attempting to load from .obj" << std::endl;
//
//    // Try to load from original asset file using Assimp
//    Assimp::Importer importer;
//
//#ifdef __ANDROID__
//    // Set up custom IOSystem for Android AssetManager to access MTL files
//    directory = assetPathFS.parent_path().generic_string();
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] LoadResource: Setting up AndroidIOSystem with base dir: %s", directory.c_str());
//    importer.SetIOHandler(new AndroidIOSystem(directory));
//
//    // On Android, we need to pass just the filename to Assimp since our IOSystem handles the full path
//    std::string filename = assetPathFS.filename().generic_string();
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] LoadResource: Loading OBJ file: %s", filename.c_str());
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] About to call importer.ReadFile with filename: %s", filename.c_str());
//    const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs);
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] ReadFile completed, scene pointer: %p", scene);
//#else
//    // Check if we need to get file contents for Android
//    const aiScene* scene = nullptr;
//    scene = importer.ReadFile(assetPath, aiProcess_Triangulate | aiProcess_FlipUVs);
//#endif
//
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Scene validation - scene:%p flags:%u rootNode:%p",
//        scene, scene ? scene->mFlags : 0, scene ? scene->mRootNode : nullptr);
//#endif
//    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
//        std::cerr << "[MODEL] ERROR:ASSIMP:: " << importer.GetErrorString() << std::endl;
//#ifdef ANDROID
//        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] ERROR:ASSIMP:: %s", importer.GetErrorString());
//#endif
//        return false;
//    }
//
//#ifdef __ANDROID__
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Scene validation passed, proceeding to ProcessNode");
//#endif
//
//    // Set directory for texture loading
//    directory = assetPathFS.parent_path().generic_string();
//
//    // Process the loaded scene
//    ProcessNode(scene->mRootNode, scene);
//
//#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Successfully loaded model from .obj: %s", assetPath.c_str());
//#endif
//    std::cout << "[MODEL] Successfully loaded model from .obj: " << assetPath << std::endl;

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



// Helper functions for Bones
void Model::SetVertexBoneDataToDefault(Vertex& vertex)
{
    for (int i = 0; i < MaxBoneInfluences; i++)
    {
        vertex.mBoneIDs[i] = -1;
        vertex.mWeights[i] = 0.0f;
    }
}

void Model::SetVertexBoneData(Vertex& vertex, int boneID, float weight)
{
    for (int i = 0; i < MaxBoneInfluences; ++i)
    {
        if (vertex.mBoneIDs[i] < 0)
        {
            vertex.mBoneIDs[i] = boneID;
            vertex.mWeights[i] = weight;
            break;
        }
    }
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

void Model::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
{
    for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
    {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
        if (mBoneInfoMap.find(boneName) == mBoneInfoMap.end())
        {
            BoneInfo newBoneInfo;
            newBoneInfo.id = mBoneCounter;
			newBoneInfo.offset = aiMatrix4x4ToGlm(mesh->mBones[boneIndex]->mOffsetMatrix); // Transpose for glm
            mBoneInfoMap[boneName] = newBoneInfo;
            boneID = mBoneCounter;
            mBoneCounter++;
        }
        else
        {
			boneID = mBoneInfoMap[boneName].id;
        }

		assert(boneID != -1);
        auto weights = mesh->mBones[boneIndex]->mWeights;
        int numWeights = mesh->mBones[boneIndex]->mNumWeights;

        for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
        {
            int vertexID = weights[weightIndex].mVertexId;
            float weight = weights[weightIndex].mWeight;
            assert(vertexID <= vertices.size());
            SetVertexBoneData(vertices[vertexID], boneID, weight);
		}
    }
}
