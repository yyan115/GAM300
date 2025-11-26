#include "pch.h"
#include "Graphics/Model/Model.h"
#ifdef __ANDROID__
#include <android/log.h>
#include <android/asset_manager.h>
#include "Platform/AndroidPlatform.h"
#include "Graphics/stb_image.h"
#endif
#include <iostream>
#include <unordered_map>
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#include "Logging.hpp"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#ifdef EDITOR
#include <meshoptimizer.h>
#endif
#include <Animation/Animator.hpp>

#ifdef ANDROID
#include <android/log.h>
#endif


// Commented out to fix warning C4505 - unreferenced function
// Remove comments when this function is used
// static void DebugPrintSkinningStats(
//     const std::vector<Vertex>& verts,
//     const std::map<std::string, BoneInfo>& boneMap,
//     const char* meshName)
// {
//     size_t hasAny = 0;
//     float minSum = 10.f, maxSum = -10.f;
//     int   maxBoneIdSeen = -1;
//
//     for (size_t i = 0; i < verts.size(); ++i) {
//         const auto& v = verts[i];
//
//         // Count non-empty influences and clamp-sum
//         int nonEmpty = 0;
//         float sum = 0.f;
//         for (int k = 0; k < MaxBoneInfluences; ++k) {
//             if (v.mBoneIDs[k] >= 0 && v.mWeights[k] > 0.f) {
//                 ++nonEmpty;
//                 sum += v.mWeights[k];
//                 maxBoneIdSeen = std::max(maxBoneIdSeen, v.mBoneIDs[k]);
//             }
//         }
//
//         if (nonEmpty > 0) {
//             ++hasAny;
//             minSum = std::min(minSum, sum);
//             maxSum = std::max(maxSum, sum);
//
//             // Optional: print a few sample vertices
//             if (hasAny <= 5) {
//                 std::cout << "[Skin] vtx " << i
//                     << " IDs=(" << v.mBoneIDs[0] << "," << v.mBoneIDs[1]
//                     << "," << v.mBoneIDs[2] << "," << v.mBoneIDs[3] << ")"
//                     << " W=(" << v.mWeights[0] << "," << v.mWeights[1]
//                     << "," << v.mWeights[2] << "," << v.mWeights[3] << ")"
//                     << " sum=" << sum << "\n";
//             }
//         }
//     }
//
//     std::cout << "[Skin] Mesh '" << (meshName ? meshName : "?")
//         << "': verts=" << verts.size()
//         << " boneMapSize=" << boneMap.size()
//         << " vertsWithInfluences=" << hasAny
//         << " weightSum(min..max)=" << minSum << ".." << maxSum
//         << " maxBoneIdSeen=" << maxBoneIdSeen
//         << "\n";
// }




Model::Model() {
	// Default constructor - meshes vector is empty by default
    metaData = std::make_shared<ModelMeta>();
}

Model::Model(std::shared_ptr<AssetMeta> modelMeta) {
	metaData = static_pointer_cast<ModelMeta>(modelMeta);
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

	if (!scene || !scene->mRootNode)
	{
        ENGINE_PRINT("ERROR:ASSIMP:: ", importer.GetErrorString(), "\n");
//#ifdef __ANDROID__
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[MODEL] Assimp loading failed: %s", importer.GetErrorString());
//#endif
        return std::string{};
	}
    else if (scene->mNumMeshes == 0 && scene->mNumAnimations > 0) {
        // This is an animation file, just return its own path (no compilation required).
        if (!forAndroid) {
            return assetPath;
        }
        else {
            std::string assetPathAndroid = assetPath.substr(assetPath.find("Resources"));
            assetPathAndroid = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string();
            // Ensure parent directories exist
            std::filesystem::path outputPath(assetPathAndroid);
            std::filesystem::create_directories(outputPath.parent_path());

            try {
                // Copy the audio file to the Android assets location
                std::filesystem::copy_file(assetPath, assetPathAndroid, std::filesystem::copy_options::overwrite_existing);
            }
            catch (const std::filesystem::filesystem_error& e) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Model] Failed to copy model (animation) file for Android: ", e.what(), "\n");
                return std::string{};
            }
            return assetPathAndroid;
        }
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
        if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            LoadMaterialTexture(material, assimpMaterial, aiTextureType_DIFFUSE, "diffuse");
        if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0)
            LoadMaterialTexture(material, assimpMaterial, aiTextureType_SPECULAR, "specular");
        if (assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0)
            LoadMaterialTexture(material, assimpMaterial, aiTextureType_NORMALS, "normal");

    }

    // If no material was created, use a default one
    if (!material)
    {
        material = Material::CreateDefault();
    }

	// Extract bone weights for vertices
	ExtractBoneWeightForVertices(vertices, mesh, scene);

    // Compile the material for the mesh if it hasn't been compiled before yet.
    std::string materialPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Materials/" + modelName + "_" + material->GetName() + ".mat";
    material->SetName(modelName + "_" + material->GetName());
    if (!AssetManager::GetInstance().IsAssetCompiled(materialPath)) {
        AssetManager::GetInstance().CompileUpdatedMaterial(materialPath, material, true);
    }

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
		texPathObj = texPathObj.stem().generic_string() + texPathObj.extension().generic_string(); // Sanitize path

        std::string texturePath = AssetManager::GetInstance().GetAssetPathFromAssetName(texPathObj.generic_string());
		texPathObj = texturePath;
        if (!std::filesystem::exists(texPathObj)) {
            ENGINE_LOG_WARN("[Model] WARNING: Texture file does not exist: " + texturePath + "\n");
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

std::string Model::CompileToMesh(const std::string& modelPathParam, std::vector<Mesh>& meshesToCompile, bool forAndroid) {
#ifdef EDITOR
    // Optimize the meshes.
    if (metaData->optimizeMeshes) {
        for (auto& mesh : meshesToCompile) {
            // Remove redundant vertices (e.g. the same position, normal, UV, etc.) and reindex the mesh.
            std::vector<unsigned int> remap(mesh.indices.size());
            size_t vertex_count = meshopt_generateVertexRemap(
                remap.data(),
                mesh.indices.data(), mesh.indices.size(),
                mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex));

            std::vector<Vertex> newVertices(vertex_count);
            std::vector<unsigned int> newIndices(mesh.indices.size());

            meshopt_remapVertexBuffer(newVertices.data(), mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex), remap.data());
            meshopt_remapIndexBuffer(newIndices.data(), mesh.indices.data(), mesh.indices.size(), remap.data());

            mesh.vertices.swap(newVertices);
            mesh.indices.swap(newIndices);

            // Run vertex cache optimization.
            // Reduces the number of vertex shader invocations by reordering triangles.
            meshopt_optimizeVertexCache(mesh.indices.data(), mesh.indices.data(), mesh.indices.size(), mesh.vertices.size());

            // Run overdraw optimization.
            // Reduces overdraw by reordering triangles.
            meshopt_optimizeOverdraw(mesh.indices.data(), mesh.indices.data(), mesh.indices.size(), &mesh.vertices[0].position.x, mesh.vertices.size(), sizeof(Vertex), 1.05f);

            // Run vertex fetch optimization.
            // Optimizes the vertex buffer for GPU vertex fetch.
            meshopt_optimizeVertexFetch(mesh.vertices.data(), mesh.indices.data(), mesh.indices.size(), mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex));
        }
    }
#endif

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
				meshFile.write(reinterpret_cast<const char*>(v.mBoneIDs), sizeof(v.mBoneIDs));
				meshFile.write(reinterpret_cast<const char*>(v.mWeights), sizeof(v.mWeights));
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

        // Write BoneInfo map for the model
        {
            bool hasBones = !mBoneInfoMap.empty();
            meshFile.write(reinterpret_cast<const char*>(&hasBones), sizeof(hasBones));

            if (hasBones)
            {
                // Write the number of bones
                meshFile.write(reinterpret_cast<const char*>(&mBoneCounter), sizeof(mBoneCounter));

                // Write each bone's name and offset matrix
                for (const auto& [name, info] : mBoneInfoMap)
                {
                    //// LOG BEFORE WRITING
                    //if (name == "mixamorig:Hips" || name == "mixamorig:Spine") {
                    //    ENGINE_LOG_DEBUG("[WriteBone] '" + name + "' ID=" + std::to_string(info.id) + " Offset: [" +
                    //        std::to_string(info.offset[0][0]) + " " + std::to_string(info.offset[1][0]) + " " + std::to_string(info.offset[2][0]) + " " + std::to_string(info.offset[3][0]) + "] [" +
                    //        std::to_string(info.offset[0][1]) + " " + std::to_string(info.offset[1][1]) + " " + std::to_string(info.offset[2][1]) + " " + std::to_string(info.offset[3][1]) + "] [" +
                    //        std::to_string(info.offset[0][2]) + " " + std::to_string(info.offset[1][2]) + " " + std::to_string(info.offset[2][2]) + " " + std::to_string(info.offset[3][2]) + "] [" +
                    //        std::to_string(info.offset[0][3]) + " " + std::to_string(info.offset[1][3]) + " " + std::to_string(info.offset[2][3]) + " " + std::to_string(info.offset[3][3]) + "]\n");
                    //}

                    size_t nameLen = name.size();
                    meshFile.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));   // Size of the name
                    meshFile.write(name.data(), nameLen);   // Actual name string
					meshFile.write(reinterpret_cast<const char*>(&info.id), sizeof(info.id));         // Bone ID
                    meshFile.write(reinterpret_cast<const char*>(&info.offset), sizeof(info.offset)); // Offset matrix
                }
            }

			// Clear bone info after writing
			mBoneInfoMap.clear();
			mBoneCounter = 0;
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
				std::memcpy(&v.mBoneIDs, buffer.data() + offset, sizeof(v.mBoneIDs));
				offset += sizeof(v.mBoneIDs);
				std::memcpy(&v.mWeights, buffer.data() + offset, sizeof(v.mWeights));
				offset += sizeof(v.mWeights);

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

            Mesh newMesh(vertices, indices, material);
            newMesh.CalculateBoundingBox();
            meshes.push_back(std::move(newMesh));
        }

        // Read BoneInfo Map
        {
			// Check if model has bones
            bool hasBones = false;
			std::memcpy(&hasBones, buffer.data() + offset, sizeof(hasBones));
            offset += sizeof(hasBones);

			// Clear existing bone info
			mBoneInfoMap.clear();
			mBoneCounter = 0;

            if (hasBones)
            {
                // Read the number of bones
                std::memcpy(&mBoneCounter, buffer.data() + offset, sizeof(mBoneCounter));
                offset += sizeof(mBoneCounter);

                // Read each bone's name and offset matrix
                for (int i = 0; i < mBoneCounter; ++i)
                {
					// Get bone name
					size_t nameLen = 0;
                    std::memcpy(&nameLen, buffer.data() + offset, sizeof(nameLen));
                    offset += sizeof(nameLen);

					// Get actual name string
                    std::string name(nameLen, '\0');
                    std::memcpy(&name[0], buffer.data() + offset, nameLen);
                    offset += nameLen;

                    // Get Bone Info
                    BoneInfo boneInfo;
					memcpy(&boneInfo.id, buffer.data() + offset, sizeof(boneInfo.id));
					offset += sizeof(boneInfo.id);
					memcpy(&boneInfo.offset, buffer.data() + offset, sizeof(boneInfo.offset));
					offset += sizeof(boneInfo.offset);
					mBoneInfoMap[name] = boneInfo;
                }
			}

            //for (auto& [name, info] : mBoneInfoMap) {
            //    if (name == "mixamorig:Hips" || name == "mixamorig:Spine") {
            //        ENGINE_LOG_DEBUG("[LoadBone] '" + name + "' ID=" + std::to_string(info.id) + " Offset: [" +
            //            std::to_string(info.offset[0][0]) + " " + std::to_string(info.offset[1][0]) + " " + std::to_string(info.offset[2][0]) + " " + std::to_string(info.offset[3][0]) + "] [" +
            //            std::to_string(info.offset[0][1]) + " " + std::to_string(info.offset[1][1]) + " " + std::to_string(info.offset[2][1]) + " " + std::to_string(info.offset[3][1]) + "] [" +
            //            std::to_string(info.offset[0][2]) + " " + std::to_string(info.offset[1][2]) + " " + std::to_string(info.offset[2][2]) + " " + std::to_string(info.offset[3][2]) + "] [" +
            //            std::to_string(info.offset[0][3]) + " " + std::to_string(info.offset[1][3]) + " " + std::to_string(info.offset[2][3]) + " " + std::to_string(info.offset[3][3]) + "]\n");
            //    }
            //}
        }

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
    std::string metaFilePath{};
    if (!forAndroid) {
        metaFilePath = assetPath + ".meta";
    }
    else {
        std::string assetPathAndroid = assetPath.substr(assetPath.find("Resources"));
        metaFilePath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + ".meta";
    }
    std::ifstream ifs(metaFilePath);
    std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());
    ifs.close();

    auto& allocator = doc.GetAllocator();

    rapidjson::Value modelMetaData(rapidjson::kObjectType);

    modelMetaData.AddMember("optimizeMeshes", rapidjson::Value().SetBool(metaData->optimizeMeshes), allocator);

    doc.AddMember("ModelMetaData", modelMetaData, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream metaFile(metaFilePath);
    metaFile << buffer.GetString();
    metaFile.close();

    std::shared_ptr<ModelMeta> newMetaData = std::make_shared<ModelMeta>();
    newMetaData->PopulateAssetMeta(currentMetaData->guid, currentMetaData->sourceFilePath, currentMetaData->compiledFilePath, currentMetaData->version);
    newMetaData->PopulateModelMeta(metaData->optimizeMeshes);
    return newMetaData;
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


    bool isAnim = false;

	shader.setBool("isAnimated", isAnim);

	for (size_t i = 0; i < meshes.size(); ++i)
	{
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MODEL] Drawing mesh %zu/%zu with entity material", i+1, meshes.size());
//#endif
		// Use entity material if available, otherwise use mesh default
        if (entityMaterial) 
        {
            std::shared_ptr<Material> originalMaterial = meshes[i].material;
            meshes[i].material = entityMaterial;  // Set entity material
            meshes[i].Draw(shader, camera);       // Draw with entity material
            meshes[i].material = originalMaterial; // Restore original
        }
        else 
        {
            // No override, use mesh's default material
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

void Model::Draw(Shader& shader, const Camera& camera, std::shared_ptr<Material> entityMaterial, const Animator* animator)
{
    bool isAnim = false;
    const std::vector<glm::mat4>* finalBoneMatrices = nullptr;

    if (animator)
    {
        const auto& mats = animator->GetFinalBoneMatrices();
        if (!mats.empty()) 
        {
            isAnim = true;
            finalBoneMatrices = &mats;
        }
    }

	shader.setBool("isAnimated", isAnim);

    if (isAnim) 
    {
        constexpr size_t MAX_BONES = 100;
        const auto& t = *finalBoneMatrices;
        const size_t n = std::min(t.size(), MAX_BONES);
        // Upload as you already do (or via one glUniformMatrix4fv with [0])
        for (size_t i = 0; i < n; ++i)
            shader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", t[i]);
    }


    for (size_t i = 0; i < meshes.size(); ++i)
    {
        // Use entity material if available, otherwise use mesh default
        std::shared_ptr<Material> meshMaterial = entityMaterial ? entityMaterial : meshes[i].material;
        if (meshMaterial && meshMaterial != meshes[i].material) {
            // Temporarily override the mesh material for this draw call
            std::shared_ptr<Material> originalMaterial = meshes[i].material;
            meshes[i].material = meshMaterial;
            meshes[i].Draw(shader, camera);
            meshes[i].material = originalMaterial; // Restore original
        }
        else {
            meshes[i].Draw(shader, camera);
        }
    }

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
    // try empty slot first
    for (int i = 0; i < MaxBoneInfluences; ++i) {
        if (vertex.mBoneIDs[i] < 0) { vertex.mBoneIDs[i] = boneID; vertex.mWeights[i] = weight; return; }
    }
    // otherwise keep the top-4 weights
    int minI = 0;
    for (int i = 1; i < MaxBoneInfluences; ++i)
        if (vertex.mWeights[i] < vertex.mWeights[minI]) minI = i;

    if (weight > vertex.mWeights[minI]) { vertex.mBoneIDs[minI] = boneID; vertex.mWeights[minI] = weight; }
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
    scene;
    // 1) assign influences
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
    {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        if (mBoneInfoMap.find(boneName) == mBoneInfoMap.end()) {
            BoneInfo info;
            info.id = mBoneCounter;
            info.offset = aiMatrix4x4ToGlm(mesh->mBones[boneIndex]->mOffsetMatrix);

            //// LOG WHEN BONE OFFSET IS FIRST EXTRACTED
            //if (boneName == "mixamorig:Hips" || boneName == "mixamorig:Spine") {
            //    ENGINE_LOG_DEBUG("[ExtractBone] '" + boneName + "' ID=" + std::to_string(info.id) + " Offset: [" +
            //        std::to_string(info.offset[0][0]) + " " + std::to_string(info.offset[1][0]) + " " + std::to_string(info.offset[2][0]) + " " + std::to_string(info.offset[3][0]) + "] [" +
            //        std::to_string(info.offset[0][1]) + " " + std::to_string(info.offset[1][1]) + " " + std::to_string(info.offset[2][1]) + " " + std::to_string(info.offset[3][1]) + "] [" +
            //        std::to_string(info.offset[0][2]) + " " + std::to_string(info.offset[1][2]) + " " + std::to_string(info.offset[2][2]) + " " + std::to_string(info.offset[3][2]) + "] [" +
            //        std::to_string(info.offset[0][3]) + " " + std::to_string(info.offset[1][3]) + " " + std::to_string(info.offset[2][3]) + " " + std::to_string(info.offset[3][3]) + "]\n");
            //}

            mBoneInfoMap[boneName] = info;
            boneID = mBoneCounter++;
        }
        else {
            boneID = mBoneInfoMap[boneName].id;
        }

        auto* weights = mesh->mBones[boneIndex]->mWeights;
        int numWeights = mesh->mBones[boneIndex]->mNumWeights;
        for (int wi = 0; wi < numWeights; ++wi) {
            int   vtx = weights[wi].mVertexId;
            float w = weights[wi].mWeight;
            assert(vtx < static_cast<int>(vertices.size()));
            SetVertexBoneData(vertices[vtx], boneID, w);
        }
    }

    // 2) normalize each vertex's 4 weights
    for (auto& v : vertices) {
        float s = v.mWeights[0] + v.mWeights[1] + v.mWeights[2] + v.mWeights[3];
        if (s > 0.0f) {
            for (int i = 0; i < MaxBoneInfluences; ++i)
                v.mWeights[i] /= s;
        }
    }
}


