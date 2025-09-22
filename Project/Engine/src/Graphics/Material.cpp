#include "pch.h"
#include "Graphics/Material.hpp"
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

const std::unordered_map<TextureType, std::unique_ptr<TextureInfo>>& Material::GetAllTextureInfo()
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

	std::cout << "[MATERIAL] DEBUG: Texture flags - diffuse:" << hasDiffuse << " specular:" << hasSpecular << " normal:" << hasNormal << " emissive:" << hasEmissive << std::endl;
	std::cout << "[MATERIAL] DEBUG: Total texture info entries: " << m_textureInfo.size() << std::endl;

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
	for (const auto& [type, textureInfo] : m_textureInfo)
	{
		std::cout << "[MATERIAL] DEBUG: Processing texture type " << (int)type << ", textureInfo valid: " << (textureInfo != nullptr) << std::endl;
		if (textureInfo) {
			std::cout << "[MATERIAL] DEBUG: Texture ID: " << textureInfo->texture->ID << std::endl;
		}

		if (textureInfo && textureUnit < 16)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			std::cout << "[MATERIAL] DEBUG: Activating texture unit " << textureUnit << std::endl;

			textureInfo->texture->Bind(textureUnit);
			std::cout << "[MATERIAL] DEBUG: Bound texture to unit " << textureUnit << std::endl;

			std::string uniformName = "material." + TextureTypeToString(type);
			shader.setInt(uniformName.c_str(), textureUnit);
			std::cout << "[MATERIAL] DEBUG: Set uniform '" << uniformName << "' to " << textureUnit << std::endl;

			textureUnit++;
		}
	}
	std::cout << "[MATERIAL] DEBUG: Finished binding, total units used: " << textureUnit << std::endl;
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
	std::cout << "Material: " << m_name << std::endl;
	std::cout << "  Ambient: (" << m_ambient.x << ", " << m_ambient.y << ", " << m_ambient.z << ")" << std::endl;
	std::cout << "  Diffuse: (" << m_diffuse.x << ", " << m_diffuse.y << ", " << m_diffuse.z << ")" << std::endl;
	std::cout << "  Specular: (" << m_specular.x << ", " << m_specular.y << ", " << m_specular.z << ")" << std::endl;
	std::cout << "  Has Diffuse Map: " << HasTexture(TextureType::DIFFUSE) << std::endl;
	std::cout << "  Has Specular Map: " << HasTexture(TextureType::SPECULAR) << std::endl;
}
