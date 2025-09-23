#include "pch.h"
#include "Graphics/Model/Model.h"
#include "Graphics/TextureManager.h"
#include <iostream>
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"

std::string Model::CompileToResource(const std::string& assetPath)
{
	Assimp::Importer importer;
	// The function expects a file path and several post-processing options as its second argument
	// aiProcess_Triangulate tells Assimp that if the model does not (entirely) consist of triangles, it should transform all the model's primitive shapes to triangles first.
	const aiScene* scene = importer.ReadFile(assetPath, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR:ASSIMP:: " << importer.GetErrorString() << std::endl;
        return std::string{};
	}

	directory = assetPath.substr(0, assetPath.find_last_of('/'));

	// Recursive function
	ProcessNode(scene->mRootNode, scene);

	return CompileToMesh(assetPath, meshes);
}

void Model::ProcessNode(aiNode* node, const aiScene* scene)
{
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
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<std::shared_ptr<Texture>> textures;

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

        // Diffuse textures
        std::vector<std::shared_ptr<Texture>> diffuseMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_DIFFUSE, "diffuse");
        //if (!diffuseMaps.empty()) {
        //    material->SetTexture(TextureType::DIFFUSE, diffuseMaps[0]);
        //}

        // Specular textures
        std::vector<std::shared_ptr<Texture>> specularMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_SPECULAR, "specular");
        //if (!specularMaps.empty()) 
        //{
        //    material->SetTexture(TextureType::SPECULAR, specularMaps[0]);
        //}

        // Normal maps
        std::vector<std::shared_ptr<Texture>> normalMaps = LoadMaterialTexture(material, assimpMaterial, aiTextureType_NORMALS, "normal");
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
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    }

    // If no material was created, use a default one
    if (!material) 
    {
        material = Material::CreateDefault();
    }

	// Extract bone weights for vertices
	ExtractBoneWeightForVertices(vertices, mesh, scene);

    return Mesh(vertices, indices, material);
}

std::vector<std::shared_ptr<Texture>> Model::LoadMaterialTexture(std::shared_ptr<Material> material, aiMaterial* mat, aiTextureType type, std::string typeName)
{
	typeName;
	std::vector<std::shared_ptr<Texture>> textures;
	//TextureManager& textureManager = TextureManager::getInstance();

	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		std::string texturePath = directory + '/' + str.C_Str();

		// Use the asset manager
        if (AssetManager::GetInstance().CompileTexture(texturePath, typeName, -1)) {
		    auto texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
		    //auto texture = textureManager.loadTexture(texturePath, typeName);
		    if (texture) 
		    {
			    textures.push_back(texture); // Dereference shared_ptr

			    std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
                material->SetTexture(static_cast<TextureType>(type), std::move(textureInfo));
		    }            
		}
	}

	return textures;
}

std::string Model::CompileToMesh(const std::string& modelPath, const std::vector<Mesh>& meshesToCompile) {
    std::filesystem::path p(modelPath);
    std::string meshPath = (p.parent_path() / p.stem()).generic_string() + ".mesh";

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

        return meshPath;
    }

    return std::string{};
}

bool Model::LoadResource(const std::string& assetPath)
{
    std::filesystem::path assetPathFS(assetPath);
    std::string resourcePath = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".mesh";

	std::ifstream meshFile(resourcePath, std::ios::binary);
    if (meshFile.is_open()) {
		// Read the number of meshes from the file.
        size_t meshCount;
		meshFile.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));

        // For each mesh, read its data from the file.
        for (size_t i = 0; i < meshCount; ++i) {
            size_t vertexCount, indexCount;
            // Read vertex and index count from the file.
            meshFile.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
			meshFile.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

            // Read vertex data from the file.
			std::vector<Vertex> vertices(vertexCount);
            for (size_t j = 0; j < vertexCount; ++j) {
                Vertex v;
                meshFile.read(reinterpret_cast<char*>(&v.position), sizeof(v.position));
				meshFile.read(reinterpret_cast<char*>(&v.normal), sizeof(v.normal));
				meshFile.read(reinterpret_cast<char*>(&v.color), sizeof(v.color));
                meshFile.read(reinterpret_cast<char*>(&v.texUV), sizeof(v.texUV));
				vertices[j] = std::move(v);
            }

			// Read index data from the file.
            std::vector<GLuint> indices(indexCount);
			meshFile.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(GLuint));

            // Read material properties from the file.
			std::shared_ptr<Material> material = std::make_shared<Material>();
            // Name
			size_t nameLength;
			meshFile.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
            std::string meshName(nameLength, '\0'); // Pre-size the string
			meshFile.read(reinterpret_cast<char*>(&meshName[0]), nameLength);
			material->SetName(meshName);
            // Ambient
            glm::vec3 ambient;
			meshFile.read(reinterpret_cast<char*>(&ambient), sizeof(ambient));
			material->SetAmbient(ambient);
            // Diffuse
			glm::vec3 diffuse;
			meshFile.read(reinterpret_cast<char*>(&diffuse), sizeof(diffuse));
			material->SetDiffuse(diffuse);
			// Specular
			glm::vec3 specular;
			meshFile.read(reinterpret_cast<char*>(&specular), sizeof(specular));
			material->SetSpecular(specular);
            // Emissive
            glm::vec3 emissive;
			meshFile.read(reinterpret_cast<char*>(&emissive), sizeof(emissive));
            material->SetEmissive(emissive);
            // Shininess
			float shininess;
			meshFile.read(reinterpret_cast<char*>(&shininess), sizeof(shininess));
            material->SetShininess(shininess);
			// Opacity
			float opacity;
            meshFile.read(reinterpret_cast<char*>(&opacity), sizeof(opacity));
			material->SetOpacity(opacity);
            // Metallic
			float metallic;
			meshFile.read(reinterpret_cast<char*>(&metallic), sizeof(metallic));
            material->SetMetallic(metallic);
			// Roughness
			float roughness;
            meshFile.read(reinterpret_cast<char*>(&roughness), sizeof(roughness));
			material->SetRoughness(roughness);
			// AO
            float ao;
			meshFile.read(reinterpret_cast<char*>(&ao), sizeof(ao));
			material->SetAO(ao);

			// Read texture paths from the file.
			size_t textureCount;
            std::vector<std::shared_ptr<Texture>> textures;
            meshFile.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));
            for (size_t j = 0; j < textureCount; ++j) {
                TextureType texType;
				meshFile.read(reinterpret_cast<char*>(&texType), sizeof(texType));
				size_t pathLength;
				meshFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
				std::string texturePath(pathLength, '\0');
				meshFile.read(reinterpret_cast<char*>(&texturePath[0]), pathLength);

                // Load texture via Resource Manager
				std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
                if (texture) {
                    std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
                    material->SetTexture(texType, std::move(textureInfo));

                    // Assign the texture type
                    switch (texType) {
                        case TextureType::DIFFUSE:
                            texture->type = "diffuse";
							break;
                        case TextureType::SPECULAR:
							texture->type = "specular";
                            break;
						case TextureType::NORMAL:
                            texture->type = "normal";
                            break;
						case TextureType::EMISSIVE:
                            texture->type = "emissive";
                            break;
                        // Add other cases as needed
                        default:
							std::cerr << "[MODEL] Warning: Unhandled texture type in model loading.\n";
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

    return false;
}

std::shared_ptr<AssetMeta> Model::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData)
{
    assetPath, currentMetaData;
    return std::shared_ptr<AssetMeta>();
}

void Model::Draw(Shader& shader, const Camera& camera)
{
	for (auto& mesh : meshes)
	{
		mesh.Draw(shader, camera);
	}
}


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

