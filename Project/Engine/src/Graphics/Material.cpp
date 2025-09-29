#include "pch.h"
#include "Graphics/Material.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef __ANDROID__
#include <android/log.h>
#endif

Material::Material() : m_name("DefaultMaterial") {
}

Material::Material(const std::string& name) : m_name(name) {
}

void Material::SetAmbient(const glm::vec3 ambient)
{
	m_ambient = ambient;
}

void Material::SetDiffuse(const glm::vec3& diffuse)
{
	m_diffuse = diffuse;
}

void Material::SetSpecular(const glm::vec3& specular)
{
	m_specular = specular;
}

void Material::SetEmissive(const glm::vec3& emissive)
{
	m_emissive = emissive;
}

void Material::SetShininess(float shininess)
{
	m_shininess = glm::clamp(shininess, 1.f, 256.f);
}

void Material::SetOpacity(float opacity)
{
	m_opacity = glm::clamp(opacity, 0.f, 1.f);
}

void Material::SetMetallic(float metallic)
{
	m_metallic = glm::clamp(metallic, 0.f, 1.f);
}

void Material::SetRoughness(float roughness)
{
	m_roughness = glm::clamp(roughness, 0.f, 1.f);
}

void Material::SetAO(float ao)
{
	m_ao = glm::clamp(ao, 0.f, 1.f);
}

void Material::SetTexture(TextureType type, std::unique_ptr<TextureInfo> textureInfo)
{
	if (textureInfo)
	{
		m_textureInfo[type] = std::move(textureInfo);
	}
}


std::optional<std::reference_wrapper<TextureInfo>> Material::GetTextureInfo(TextureType type) const
{
	std::optional<std::reference_wrapper<TextureInfo>> textureInfo = std::nullopt;
	auto it = m_textureInfo.find(type);
	if (it != m_textureInfo.end()) {
		textureInfo = *(it->second);
	}

	return textureInfo;
}

const std::unordered_map<Material::TextureType, std::unique_ptr<TextureInfo>>& Material::GetAllTextureInfo()
{
	return m_textureInfo;
}

bool Material::HasTexture(TextureType type) const
{
	return m_textureInfo.find(type) != m_textureInfo.end();
}

void Material::RemoveTexture(TextureType type)
{
	m_textureInfo.erase(type);
}

void Material::SetName(const std::string& name)
{
	m_name = name;
}

const std::string& Material::GetName() const
{
	return m_name;
}

void Material::ApplyToShader(Shader& shader) const
{
#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MATERIAL] Applying material %s - diffuse:(%.2f,%.2f,%.2f) ambient:(%.2f,%.2f,%.2f)",
		m_name.c_str(), m_diffuse.x, m_diffuse.y, m_diffuse.z, m_ambient.x, m_ambient.y, m_ambient.z);
#endif
	// Apply basic material properties
	shader.setVec3("material.ambient", m_ambient);
	shader.setVec3("material.diffuse", m_diffuse);
	shader.setVec3("material.specular", m_specular);
	shader.setVec3("material.emissive", m_emissive);
	shader.setFloat("material.shininess", m_shininess);
	shader.setFloat("material.opacity", m_opacity);

	// Apply PBR properties - For Future Use
	//shader.setFloat("material.metallic", m_metallic);
	//shader.setFloat("material.roughness", m_roughness);
	//shader.setFloat("material.ao", m_ao);

	// Bind textures
	BindTextures(shader);
}

void Material::BindTextures(Shader& shader) const
{
	// Reset texture units to be safe
	glActiveTexture(GL_TEXTURE0);
	unsigned int textureUnit = 0;

	// Set texture availability flags
	bool hasDiffuse = HasTexture(TextureType::DIFFUSE);
	bool hasSpecular = HasTexture(TextureType::SPECULAR);
	bool hasNormal = HasTexture(TextureType::NORMAL);
	bool hasEmissive = HasTexture(TextureType::EMISSIVE);

	//std::cout << "[MATERIAL] DEBUG: Texture flags - diffuse:" << hasDiffuse << " specular:" << hasSpecular << " normal:" << hasNormal << " emissive:" << hasEmissive << std::endl;
	//std::cout << "[MATERIAL] DEBUG: Total texture info entries: " << m_textureInfo.size() << std::endl;

#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MATERIAL] Texture flags - diffuse:%d specular:%d normal:%d emissive:%d",
		hasDiffuse, hasSpecular, hasNormal, hasEmissive);
#endif

	shader.setBool("material.hasDiffuseMap", hasDiffuse);
	shader.setBool("material.hasSpecularMap", hasSpecular);
	shader.setBool("material.hasNormalMap", hasNormal);
	shader.setBool("material.hasEmissiveMap", hasEmissive);
	// For Future Use
	/*shader.setBool("material.hasHeightMap", hasTexture(TextureType::HEIGHT));
	shader.setBool("material.hasAOMap", hasTexture(TextureType::AMBIENT_OCCLUSION));
	shader.setBool("material.hasMetallicMap", hasTexture(TextureType::METALLIC));
	shader.setBool("material.hasRoughnessMap", hasTexture(TextureType::ROUGHNESS));*/

	// Bind each texture type
	for (auto it = m_textureInfo.begin(); it != m_textureInfo.end(); ++it)
	{
		const TextureType& type = it->first;
		const std::unique_ptr<TextureInfo>& textureInfo = it->second;

		//std::cout << "[MATERIAL] DEBUG: Processing texture type " << (int)type << ", textureInfo valid: " << (textureInfo != nullptr) << std::endl;
		if (textureInfo) {
			//std::cout << "[MATERIAL] DEBUG: Texture ID: " << textureInfo->texture->ID << std::endl;
		}

		if (textureInfo && textureInfo->texture && textureUnit < 16)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			//std::cout << "[MATERIAL] DEBUG: Activating texture unit " << textureUnit << std::endl;

			textureInfo->texture->Bind(textureUnit);
			//std::cout << "[MATERIAL] DEBUG: Bound texture to unit " << textureUnit << std::endl;

			std::string uniformName = "material." + TextureTypeToString(type);
			shader.setInt(uniformName.c_str(), textureUnit);
			//std::cout << "[MATERIAL] DEBUG: Set uniform '" << uniformName << "' to " << textureUnit << std::endl;

			textureUnit++;
		}
	}
	//std::cout << "[MATERIAL] DEBUG: Finished binding, total units used: " << textureUnit << std::endl;
}

std::shared_ptr<Material> Material::CreateDefault()
{
	auto material = std::make_shared<Material>("DefaultMaterial");
	material->SetAmbient(glm::vec3(0.2f, 0.2f, 0.2f));
	material->SetDiffuse(glm::vec3(0.8f, 0.8f, 0.8f));
	material->SetSpecular(glm::vec3(1.0f, 1.0f, 1.0f));
	material->SetShininess(32.0f);
	return material;
}

std::shared_ptr<Material> Material::CreateMetal(const glm::vec3& color)
{
	auto material = std::make_shared<Material>("MetalMaterial");
	material->SetAmbient(color * 0.1f);
	material->SetDiffuse(color * 0.3f);
	material->SetSpecular(glm::vec3(1.0f, 1.0f, 1.0f));
	material->SetShininess(128.0f);
	material->SetMetallic(1.0f);
	material->SetRoughness(0.1f);
	return material;
}

std::shared_ptr<Material> Material::CreatePlastic(const glm::vec3& color)
{
	auto material = std::make_shared<Material>("PlasticMaterial");
	material->SetAmbient(color * 0.2f);
	material->SetDiffuse(color);
	material->SetSpecular(glm::vec3(0.5f, 0.5f, 0.5f));
	material->SetShininess(32.0f);
	material->SetMetallic(0.0f);
	material->SetRoughness(0.5f);
	return material;
}

std::shared_ptr<Material> Material::CreateWood()
{
	auto material = std::make_shared<Material>("WoodMaterial");
	glm::vec3 woodColor(0.6f, 0.4f, 0.2f);
	material->SetAmbient(woodColor * 0.3f);
	material->SetDiffuse(woodColor);
	material->SetSpecular(glm::vec3(0.1f, 0.1f, 0.1f));
	material->SetShininess(8.0f);
	material->SetMetallic(0.0f);
	material->SetRoughness(0.8f);
	return material;
}

std::string Material::TextureTypeToString(TextureType type) const
{
	switch (type) 
	{
		case TextureType::DIFFUSE: return "diffuseMap";
		case TextureType::SPECULAR: return "specularMap";
		case TextureType::NORMAL: return "normalMap";
		case TextureType::HEIGHT: return "heightMap";
		case TextureType::AMBIENT_OCCLUSION: return "aoMap";
		case TextureType::METALLIC: return "metallicMap";
		case TextureType::ROUGHNESS: return "roughnessMap";
		case TextureType::EMISSIVE: return "emissiveMap";
		default: return "unknownMap";
	}
}

void Material::DebugPrintProperties() const
{
	//std::cout << "Material: " << m_name << std::endl;
	//std::cout << "  Ambient: (" << m_ambient.x << ", " << m_ambient.y << ", " << m_ambient.z << ")" << std::endl;
	//std::cout << "  Diffuse: (" << m_diffuse.x << ", " << m_diffuse.y << ", " << m_diffuse.z << ")" << std::endl;
	//std::cout << "  Specular: (" << m_specular.x << ", " << m_specular.y << ", " << m_specular.z << ")" << std::endl;
	//std::cout << "  Has Diffuse Map: " << HasTexture(TextureType::DIFFUSE) << std::endl;
	//std::cout << "  Has Specular Map: " << HasTexture(TextureType::SPECULAR) << std::endl;
}

// Helper method for path resolution
std::filesystem::path Material::ResolveToProjectRoot(const std::filesystem::path& path) {
	std::filesystem::path resolvedPath = path;

	// Convert to absolute path to ensure we save to the correct location
	if (!resolvedPath.is_absolute()) {
		// Find the project root directory by looking for Build folder
		std::filesystem::path projectRoot = std::filesystem::current_path();
		while (projectRoot.has_parent_path()) {
			if (std::filesystem::exists(projectRoot / "Build") &&
				std::filesystem::exists(projectRoot / "Resources")) {
				break;
			}
			projectRoot = projectRoot.parent_path();
		}

		// Debug logging
		std::cout << "[Material] DEBUG - Current path: " << std::filesystem::current_path() << std::endl;
		std::cout << "[Material] DEBUG - Found project root: " << projectRoot << std::endl;
		std::cout << "[Material] DEBUG - Input path: " << path << std::endl;

		resolvedPath = projectRoot / resolvedPath;
		std::cout << "[Material] DEBUG - Combined path: " << resolvedPath << std::endl;
	}

	// Normalize the path to resolve any .. components
	std::filesystem::path finalPath = std::filesystem::weakly_canonical(resolvedPath);
	std::cout << "[Material] DEBUG - Final resolved path: " << finalPath << std::endl;

	return finalPath;
}

std::string Material::CompileToResource(const std::string& assetPath, bool forAndroid) {
	std::filesystem::path p(assetPath);
	p = ResolveToProjectRoot(p);

	std::string materialPath = (p.parent_path() / p.stem()).generic_string() + ".mat";

	std::cout << "[Material] SAVE - Input path: " << assetPath << std::endl;
	std::cout << "[Material] SAVE - Computed path: " << materialPath << std::endl;
	std::cout << "[Material] SAVE - Working directory: " << std::filesystem::current_path() << std::endl;
	std::cout << "[Material] SAVE - Material name: " << m_name << std::endl;
	std::cout << "[Material] SAVE - Number of textures: " << m_textureInfo.size() << std::endl;
	std::cout << "[Material] SAVE - Ambient: (" << m_ambient.x << ", " << m_ambient.y << ", " << m_ambient.z << ")" << std::endl;

	std::ofstream materialFile(materialPath, std::ios::binary);
	if (materialFile.is_open()) {
		// Write material name
		size_t nameLength = m_name.size();
		materialFile.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
		materialFile.write(m_name.data(), nameLength);

		// Write basic material properties
		materialFile.write(reinterpret_cast<const char*>(&m_ambient), sizeof(m_ambient));
		materialFile.write(reinterpret_cast<const char*>(&m_diffuse), sizeof(m_diffuse));
		materialFile.write(reinterpret_cast<const char*>(&m_specular), sizeof(m_specular));
		materialFile.write(reinterpret_cast<const char*>(&m_emissive), sizeof(m_emissive));
		materialFile.write(reinterpret_cast<const char*>(&m_shininess), sizeof(m_shininess));
		materialFile.write(reinterpret_cast<const char*>(&m_opacity), sizeof(m_opacity));

		// Write PBR properties
		materialFile.write(reinterpret_cast<const char*>(&m_metallic), sizeof(m_metallic));
		materialFile.write(reinterpret_cast<const char*>(&m_roughness), sizeof(m_roughness));
		materialFile.write(reinterpret_cast<const char*>(&m_ao), sizeof(m_ao));

		// Write texture info
		size_t textureCount = m_textureInfo.size();
		materialFile.write(reinterpret_cast<const char*>(&textureCount), sizeof(textureCount));
		for (auto it = m_textureInfo.begin(); it != m_textureInfo.end(); ++it) {
			// Write texture type
			materialFile.write(reinterpret_cast<const char*>(&it->first), sizeof(it->first));
			// Write texture path length and path
			size_t pathLength = it->second->filePath.size();
			materialFile.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
			materialFile.write(it->second->filePath.data(), pathLength);
		}

		materialFile.close();
		return materialPath;
	}

	return std::string{};
}

bool Material::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
	std::filesystem::path resourcePathFS;

	if (!resourcePath.empty()) {
		resourcePathFS = std::filesystem::path(resourcePath);
	} else {
		// If no resource path provided, derive it from asset path
		std::filesystem::path assetPathFS(assetPath);
		assetPathFS = ResolveToProjectRoot(assetPathFS);
		resourcePathFS = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".mat";
	}

	resourcePathFS = ResolveToProjectRoot(resourcePathFS);
	std::string finalResourcePath = resourcePathFS.generic_string();

	std::cout << "[Material] LOAD - Input path: " << assetPath << std::endl;
	std::cout << "[Material] LOAD - Computed path: " << finalResourcePath << std::endl;
	std::cout << "[Material] LOAD - Working directory: " << std::filesystem::current_path() << std::endl;

	std::ifstream materialFile(finalResourcePath, std::ios::binary);
	if (materialFile.is_open()) {
		// Read material name
		size_t nameLength;
		materialFile.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
		std::string name(nameLength, '\0');
		materialFile.read(reinterpret_cast<char*>(&name[0]), nameLength);
		m_name = name;

		// Read basic material properties
		materialFile.read(reinterpret_cast<char*>(&m_ambient), sizeof(m_ambient));
		materialFile.read(reinterpret_cast<char*>(&m_diffuse), sizeof(m_diffuse));
		materialFile.read(reinterpret_cast<char*>(&m_specular), sizeof(m_specular));
		materialFile.read(reinterpret_cast<char*>(&m_emissive), sizeof(m_emissive));
		materialFile.read(reinterpret_cast<char*>(&m_shininess), sizeof(m_shininess));
		materialFile.read(reinterpret_cast<char*>(&m_opacity), sizeof(m_opacity));

		// Read PBR properties
		materialFile.read(reinterpret_cast<char*>(&m_metallic), sizeof(m_metallic));
		materialFile.read(reinterpret_cast<char*>(&m_roughness), sizeof(m_roughness));
		materialFile.read(reinterpret_cast<char*>(&m_ao), sizeof(m_ao));

		// Read texture info
		size_t textureCount;
		materialFile.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));

		// Clear existing textures
		m_textureInfo.clear();

		for (size_t i = 0; i < textureCount; ++i) {
			TextureType textureType;
			materialFile.read(reinterpret_cast<char*>(&textureType), sizeof(textureType));
			size_t pathLength;
			materialFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
			std::string texturePath(pathLength, '\0');
			materialFile.read(reinterpret_cast<char*>(&texturePath[0]), pathLength);

			// Create texture info with path - defer actual texture loading to when needed
			// This avoids meta file parsing issues and improves loading performance
			auto textureInfo = std::make_unique<TextureInfo>(texturePath, nullptr);
			m_textureInfo[textureType] = std::move(textureInfo);
			std::cout << "[Material] Restored texture: " << TextureTypeToString(textureType) << " -> " << texturePath << std::endl;
		}

		materialFile.close();
		std::cout << "[Material] Successfully loaded material: " << m_name << " with " << m_textureInfo.size() << " textures" << std::endl;
		return true;
	}

	std::cerr << "[Material] Failed to open material file: " << finalResourcePath << std::endl;
	return false;
}

bool Material::ReloadResource(const std::string& resourcePath, const std::string& assetPath) {
	return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Material::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid) {
	// Materials don't need extended meta data for now
	return currentMetaData;
}

