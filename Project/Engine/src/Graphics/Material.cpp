#include "pch.h"
#include "Graphics/Material.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <Logging.hpp>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <Logging.hpp>
#include "Asset Manager/ResourceManager.hpp"
#include <WindowManager.hpp>
#include <Platform/IPlatform.h>
#include <Asset Manager/AssetManager.hpp>
#include "Utilities/FileUtilities.hpp"

Material::Material() : m_name("DefaultMaterial") {
}

Material::Material(const std::string& name) : m_name(name) {
}

Material::Material(std::shared_ptr<AssetMeta> metaData) {
	metaData;
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
//#ifdef __ANDROID__
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MATERIAL] Applying material %s - diffuse:(%.2f,%.2f,%.2f) ambient:(%.2f,%.2f,%.2f)",
//		m_name.c_str(), m_diffuse.x, m_diffuse.y, m_diffuse.z, m_ambient.x, m_ambient.y, m_ambient.z);
//#endif
	// Apply basic material properties
	shader.setVec3("material.ambient", m_ambient);
	shader.setVec3("material.diffuse", m_diffuse);
	shader.setVec3("material.specular", m_specular);
	shader.setVec3("material.emissive", m_emissive);
	shader.setFloat("material.shininess", m_shininess);
	shader.setFloat("material.opacity", m_opacity);

	// PBR properties
	shader.setFloat("material.metallic", m_metallic);
	shader.setFloat("material.roughness", m_roughness);
	shader.setFloat("material.ao", m_ao);

	// DEBUG: Print emissive value
	//static int callCount = 0;
	//if (callCount++ % 300 == 0) { // Every 300 calls (about every 5 seconds at 60fps)
	//	ENGINE_PRINT("[Material] Applying ", m_name, " - Emissive: (",
	//		m_emissive.x, ", ", m_emissive.y, ", ", m_emissive.z, ")\n");
	//}
	// Bind textures
	BindTextures(shader);

	// Set texture wrapping options
	shader.setVec2("material.uTiling", tiling);
	shader.setVec2("material.uOffset", offset);
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
	bool hasHeight = HasTexture(TextureType::HEIGHT);
	bool hasAO = HasTexture(TextureType::AMBIENT_OCCLUSION);
	bool hasMetallic = HasTexture(TextureType::METALLIC);
	bool hasRoughness = HasTexture(TextureType::ROUGHNESS);
	bool hasOpacity = HasTexture(TextureType::OPACITY);

#if defined(ANDROID) || defined(__ANDROID__)
	// Android shader has samplers outside of Material struct (OpenGL ES compatibility)
	// Use uniform names WITHOUT "material." prefix for samplers and flags
	static bool loggedOnce = false;
	if (!loggedOnce) {
		__android_log_print(ANDROID_LOG_INFO, "GAM300", "[MATERIAL] Setting texture flags: hasDiffuse=%d hasSpecular=%d hasNormal=%d hasEmissive=%d",
			hasDiffuse, hasSpecular, hasNormal, hasEmissive);
		loggedOnce = true;
	}
	shader.setBool("hasDiffuseMap", hasDiffuse);
	shader.setBool("hasSpecularMap", hasSpecular);
	shader.setBool("hasNormalMap", hasNormal);
	shader.setBool("hasEmissiveMap", hasEmissive);
	shader.setBool("hasHeightMap", hasHeight);
	shader.setBool("hasAOMap", hasAO);
	shader.setBool("hasMetallicMap", hasMetallic);
	shader.setBool("hasRoughnessMap", hasRoughness);
	shader.setBool("hasOpacityMap", hasOpacity);
#else
	shader.setBool("material.hasDiffuseMap", hasDiffuse);
	shader.setBool("material.hasSpecularMap", hasSpecular);
	shader.setBool("material.hasNormalMap", hasNormal);
	shader.setBool("material.hasEmissiveMap", hasEmissive);
	shader.setBool("material.hasHeightMap", hasHeight);
	shader.setBool("material.hasAOMap", hasAO);
	shader.setBool("material.hasMetallicMap", hasMetallic);
	shader.setBool("material.hasRoughnessMap", hasRoughness);
	shader.setBool("material.hasOpacityMap", hasOpacity);
#endif

	// Bind each texture type
	for (auto it = m_textureInfo.begin(); it != m_textureInfo.end(); ++it)
	{
		const TextureType& type = it->first;
		const std::unique_ptr<TextureInfo>& textureInfo = it->second;

		//std::cout << "[MATERIAL] DEBUG: Processing texture type " << (int)type << ", textureInfo valid: " << (textureInfo != nullptr) << std::endl;
		if (textureInfo) {
			// Check if the texture is loaded. If it isn't, load it now.
			if (!textureInfo->texture) {
#ifndef ANDROID
				textureInfo->texture = ResourceManager::GetInstance().GetResource<Texture>(textureInfo->filePath);
#else
				std::string androidAssetPath = textureInfo->filePath.substr(textureInfo->filePath.find("Resources"));
				ENGINE_LOG_DEBUG("[Material] Getting texture from: " + androidAssetPath);
				textureInfo->texture = ResourceManager::GetInstance().GetResource<Texture>(androidAssetPath);
#endif
			}
		}

		if (textureInfo && textureInfo->texture && textureUnit < 16)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);

			textureInfo->texture->Bind(textureUnit);

#if defined(ANDROID) || defined(__ANDROID__)
			// Android shader has samplers outside of Material struct
			std::string uniformName = TextureTypeToString(type);
#else
			std::string uniformName = "material." + TextureTypeToString(type);
#endif
			shader.setInt(uniformName.c_str(), textureUnit);

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
		case TextureType::OPACITY: return "opacityMap";
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
		std::filesystem::path foundProjectRoot;

		while (projectRoot.has_parent_path()) {
			if (std::filesystem::exists(projectRoot / "Build") &&
				std::filesystem::exists(projectRoot / "Resources")) {
				foundProjectRoot = projectRoot;
				break;
			}
			projectRoot = projectRoot.parent_path();
		}

		// If we didn't find a project root, use current directory as fallback
		if (foundProjectRoot.empty()) {
			foundProjectRoot = std::filesystem::current_path();
		}

		projectRoot = foundProjectRoot;

		// For relative paths, resolve them from current directory first
		if (resolvedPath.is_relative()) {
			// Resolve relative to current working directory
			resolvedPath = std::filesystem::current_path() / resolvedPath;
		} else {
			// For absolute paths, combine with project root
			resolvedPath = projectRoot / resolvedPath;
		}
	}

	// Normalize the path to resolve any .. components
	std::filesystem::path finalPath = std::filesystem::weakly_canonical(resolvedPath);

	return finalPath;
}

bool Material::GetMaterialPropertiesFromAsset(const std::string& assetPath) {
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		std::cerr << "[SHADER] ERROR: Platform not available for asset discovery!" << std::endl;
		return false;
	}

	if (!platform->FileExists(assetPath)) {
		ENGINE_LOG_WARN("[Material] Material " + assetPath + " not found!");
		return false;
	}

	std::vector<uint8_t> buffer = platform->ReadAsset(assetPath);
	if (!buffer.empty()) {
		try {
			size_t readOffset = 0;
			// Read material properties from the file.
			// Name
			size_t nameLength;
			std::memcpy(&nameLength, buffer.data() + readOffset, sizeof(nameLength));
			readOffset += sizeof(nameLength);
			std::string meshName(nameLength, '\0'); // Pre-size the string
			std::memcpy(&meshName[0], buffer.data() + readOffset, nameLength);
			readOffset += nameLength;
			SetName(meshName);
			// Ambient
			glm::vec3 ambient;
			std::memcpy(&ambient, buffer.data() + readOffset, sizeof(ambient));
			readOffset += sizeof(ambient);
			SetAmbient(ambient);
			// Diffuse
			glm::vec3 diffuse;
			std::memcpy(&diffuse, buffer.data() + readOffset, sizeof(diffuse));
			readOffset += sizeof(diffuse);
			SetDiffuse(diffuse);
			// Specular
			glm::vec3 specular;
			std::memcpy(&specular, buffer.data() + readOffset, sizeof(specular));
			readOffset += sizeof(specular);
			SetSpecular(specular);
			// Emissive
			glm::vec3 emissive;
			std::memcpy(&emissive, buffer.data() + readOffset, sizeof(emissive));
			readOffset += sizeof(emissive);
			SetEmissive(emissive);
			// Shininess
			float shininess;
			std::memcpy(&shininess, buffer.data() + readOffset, sizeof(shininess));
			readOffset += sizeof(shininess);
			SetShininess(shininess);
			// Opacity
			float opacity;
			std::memcpy(&opacity, buffer.data() + readOffset, sizeof(opacity));
			readOffset += sizeof(opacity);
			SetOpacity(opacity);
			// Metallic
			float metallic;
			std::memcpy(&metallic, buffer.data() + readOffset, sizeof(metallic));
			readOffset += sizeof(metallic);
			SetMetallic(metallic);
			// Roughness
			float roughness;
			std::memcpy(&roughness, buffer.data() + readOffset, sizeof(roughness));
			readOffset += sizeof(roughness);
			SetRoughness(roughness);
			// AO
			float ao;
			std::memcpy(&ao, buffer.data() + readOffset, sizeof(ao));
			readOffset += sizeof(ao);
			SetAO(ao);

			// Read texture paths from the file.
			size_t textureCount;
			std::memcpy(&textureCount, buffer.data() + readOffset, sizeof(textureCount));
			readOffset += sizeof(textureCount);
			for (size_t j = 0; j < textureCount; ++j) {
				Material::TextureType texType;
				std::memcpy(&texType, buffer.data() + readOffset, sizeof(texType));
				readOffset += sizeof(texType);
				size_t pathLength;
				std::memcpy(&pathLength, buffer.data() + readOffset, sizeof(pathLength));
				readOffset += sizeof(pathLength);
				std::string texturePath(buffer.data() + readOffset, buffer.data() + readOffset + pathLength);
				// strip trailing nulls
				texturePath.erase(std::find(texturePath.begin(), texturePath.end(), '\0'), texturePath.end());
				readOffset += pathLength;

				// Texture doesn't have to be loaded now, it will only be loaded when it is being rendered.
				//std::shared_ptr<Texture> texture = std::make_shared<Texture>();
				std::unique_ptr<TextureInfo> textureInfo = std::make_unique<TextureInfo>(texturePath, nullptr);
				SetTexture(texType, std::move(textureInfo));

				//// Assign the texture type
				//switch (texType) {
				//case Material::TextureType::DIFFUSE:
				//	texture->type = "diffuse";
				//	break;
				//case Material::TextureType::SPECULAR:
				//	texture->type = "specular";
				//	break;
				//case Material::TextureType::NORMAL:
				//	texture->type = "normal";
				//	break;
				//case Material::TextureType::EMISSIVE:
				//	texture->type = "emissive";
				//	break;
				//	// Add other cases as needed
				//default:
				//	ENGINE_PRINT(EngineLogging::LogLevel::Error, "[MODEL] Warning: Unhandled texture type in model loading.\n");
				//	//std::cerr << "[MODEL] Warning: Unhandled texture type in model loading.\n";
				//	texture->type = "unknown";
				//	break;
				//}
			}

			// Ensure we don't overflow the buffer as these are new material options that not all materials might have.
			if (readOffset + (2 * sizeof(glm::vec2)) > buffer.size()) {
				return true;
			}

			// Read texture wrapping options from file.
			glm::vec2 loadedTiling;
			std::memcpy(&loadedTiling, buffer.data() + readOffset, sizeof(loadedTiling));
			readOffset += sizeof(loadedTiling);
			SetTiling(loadedTiling);

			glm::vec2 textureOffset;
			std::memcpy(&textureOffset, buffer.data() + readOffset, sizeof(textureOffset));
			readOffset += sizeof(textureOffset);
			SetOffset(textureOffset);
		}
		catch (const std::exception& e) {
			ENGINE_LOG_ERROR("[Material] Failed to load material: " + std::string(e.what()));
		}
	}

	return true;

	//std::ifstream materialFile(assetPath, std::ios::binary);
	//if (materialFile.is_open()) {
	//	// Read material name
	//	size_t nameLength;
	//	materialFile.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
	//	std::string name(nameLength, '\0');
	//	materialFile.read(reinterpret_cast<char*>(&name[0]), nameLength);
	//	m_name = name;

	//	// Read basic material properties
	//	materialFile.read(reinterpret_cast<char*>(&m_ambient), sizeof(m_ambient));
	//	materialFile.read(reinterpret_cast<char*>(&m_diffuse), sizeof(m_diffuse));
	//	materialFile.read(reinterpret_cast<char*>(&m_specular), sizeof(m_specular));
	//	materialFile.read(reinterpret_cast<char*>(&m_emissive), sizeof(m_emissive));
	//	materialFile.read(reinterpret_cast<char*>(&m_shininess), sizeof(m_shininess));
	//	materialFile.read(reinterpret_cast<char*>(&m_opacity), sizeof(m_opacity));

	//	// Read PBR properties
	//	materialFile.read(reinterpret_cast<char*>(&m_metallic), sizeof(m_metallic));
	//	materialFile.read(reinterpret_cast<char*>(&m_roughness), sizeof(m_roughness));
	//	materialFile.read(reinterpret_cast<char*>(&m_ao), sizeof(m_ao));

	//	// Read texture info
	//	size_t textureCount;
	//	materialFile.read(reinterpret_cast<char*>(&textureCount), sizeof(textureCount));

	//	// Clear existing textures
	//	m_textureInfo.clear();

	//	for (size_t i = 0; i < textureCount; ++i) {
	//		TextureType textureType;
	//		materialFile.read(reinterpret_cast<char*>(&textureType), sizeof(textureType));
	//		size_t pathLength;
	//		materialFile.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
	//		std::string texturePath(pathLength, '\0');
	//		materialFile.read(reinterpret_cast<char*>(&texturePath[0]), pathLength);

	//		// Create texture info with path - defer actual texture loading to when needed
	//		// This avoids meta file parsing issues and improves loading performance
	//		std::shared_ptr<Texture> texture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);
	//		auto textureInfo = std::make_unique<TextureInfo>(texturePath, texture);
	//		m_textureInfo[textureType] = std::move(textureInfo);
	//		std::cout << "[Material] Restored texture: " << TextureTypeToString(textureType) << " -> " << texturePath << std::endl;
	//	}

	//	materialFile.close();
	//	std::cout << "[Material] Successfully loaded material: " << m_name << " with " << m_textureInfo.size() << " textures" << std::endl;
	//	
	//	return true;
	//}

	//ENGINE_LOG_DEBUG("[Material] Material " + assetPath + " doesn't exist or can't be read.");
	//return false;
}

std::string Material::CompileToResource(const std::string& assetPath, bool forAndroid) {
	std::filesystem::path p(assetPath);
	//p = ResolveToProjectRoot(p);

	std::string materialPath = (p.parent_path() / p.stem()).generic_string() + ".mat";
	// Try to get the material info from the material asset first (if it exists).
	GetMaterialPropertiesFromAsset(materialPath);
	SetName(p.stem().generic_string());

	if (forAndroid) {
		std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
		assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
		materialPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + "_android.mat";
		std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(materialPath));
		materialPath = newPath.generic_string();
	}

	materialPath = FileUtilities::SanitizeFilePath(materialPath);
	//std::cout << "[Material] SAVE - Input path: " << assetPath << std::endl;
	//std::cout << "[Material] SAVE - Computed path: " << materialPath << std::endl;
	//std::cout << "[Material] SAVE - Working directory: " << std::filesystem::current_path() << std::endl;
	//std::cout << "[Material] SAVE - Material name: " << m_name << std::endl;
	//std::cout << "[Material] SAVE - Number of textures: " << m_textureInfo.size() << std::endl;
	//std::cout << "[Material] SAVE - Ambient: (" << m_ambient.x << ", " << m_ambient.y << ", " << m_ambient.z << ")" << std::endl;

	p = materialPath;
	std::filesystem::create_directories(p.parent_path());

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

		// Write texture wrapping options
		materialFile.write(reinterpret_cast<const char*>(&tiling), sizeof(tiling));
		materialFile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));

		materialFile.close();
		return materialPath;
	}

	return std::string{};
}

std::string Material::CompileUpdatedAssetToResource(const std::string& assetPath, bool forAndroid) {
	std::filesystem::path p(assetPath);
	//p = ResolveToProjectRoot(p);

	std::string materialPath = (p.parent_path() / p.stem()).generic_string() + ".mat";
	SetName(p.stem().generic_string());

	if (forAndroid) {
		std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
		assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
		materialPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + "_android.mat";
		std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(materialPath));
		materialPath = newPath.generic_string();
	}

	materialPath = FileUtilities::SanitizeFilePath(materialPath);
	//ENGINE_PRINT("[Material] SAVE - Input path: ", assetPath, "\n");
	//ENGINE_PRINT("[Material] SAVE - Computed path: ", materialPath, "\n");
	//ENGINE_PRINT("[Material] SAVE - Working directory: ", std::filesystem::current_path(), "\n");
	//ENGINE_PRINT("[Material] SAVE - Material name: ", m_name, "\n");
	//ENGINE_PRINT("[Material] SAVE - Number of textures: ", m_textureInfo.size(), "\n");
	//ENGINE_PRINT("[Material] SAVE - Ambient: (", m_ambient.x, ", ", m_ambient.y, ", ", m_ambient.z, ")\n");

	p = materialPath;
	std::filesystem::create_directories(p.parent_path());

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

		// Write texture wrapping options
		materialFile.write(reinterpret_cast<const char*>(&tiling), sizeof(tiling));
		materialFile.write(reinterpret_cast<const char*>(&offset), sizeof(offset));

		materialFile.close();
		return materialPath;
	}

	return std::string{};
}

bool Material::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
	//ENGINE_LOG_INFO("[Material] Loading material: " + resourcePath);
	std::filesystem::path resourcePathFS;

	if (!resourcePath.empty()) {
		resourcePathFS = std::filesystem::path(resourcePath);
	} else {
		// If no resource path provided, derive it from asset path
		std::filesystem::path assetPathFS(assetPath);
		assetPathFS = ResolveToProjectRoot(assetPathFS);
		resourcePathFS = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".mat";
	}

	//ENGINE_LOG_INFO("[Material] Resolving project root");
#ifdef EDITOR
	resourcePathFS = ResolveToProjectRoot(resourcePathFS);
#endif
	std::string finalResourcePath = resourcePathFS.generic_string();

	//ENGINE_PRINT("[Material] LOAD - Input path: ", assetPath, "\n");
	//ENGINE_PRINT("[Material] LOAD - Computed path: ", finalResourcePath, "\n");
	//ENGINE_PRINT("[Material] LOAD - Working directory: ", std::filesystem::current_path(), "\n");


	if (!GetMaterialPropertiesFromAsset(finalResourcePath)) {
		//ENGINE_LOG_INFO("[Material] Compiling material: " + assetPath);
		AssetManager::GetInstance().CompileAsset<Material>(assetPath);
		LoadResource(resourcePath, assetPath);
	}

	return true;
}

bool Material::ReloadResource(const std::string& resourcePath, const std::string& assetPath) {
	return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Material::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid) {
	(void)assetPath; // Suppress unused parameter warning
	(void)forAndroid; // Suppress unused parameter warning
	// Materials don't need extended meta data for now
	return currentMetaData;
}

