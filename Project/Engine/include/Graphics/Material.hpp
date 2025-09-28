#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <functional>
#include <fstream>
#include <filesystem>
#include "Texture.h"
#include "ShaderClass.h"
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

enum class TextureType {
	NONE = 0,
	DIFFUSE = 1,
	SPECULAR = 2,
	AMBIENT_OCCLUSION = 3,
	EMISSIVE = 4,
	HEIGHT = 5,
	NORMAL = 6,
	METALLIC = 15,
	ROUGHNESS = 16,
};

class ENGINE_API Material : public IAsset {
public:
	Material();
	Material(const std::string& name);
	~Material() = default;

	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;

	// Basic Material Properties
	void SetAmbient(const glm::vec3 ambient);
	void SetDiffuse(const glm::vec3& diffuse);
	void SetSpecular(const glm::vec3& specular);
	void SetEmissive(const glm::vec3& emissive);
	void SetShininess(float shininess);
	void SetOpacity(float opacity);

	const glm::vec3& GetAmbient() const { return m_ambient; }
	const glm::vec3& GetDiffuse() const { return m_diffuse; }
	const glm::vec3& GetSpecular() const { return m_specular; }
	const glm::vec3& GetEmissive() const { return m_emissive; }
	const float& GetShininess() const { return m_shininess; }
	const float& GetOpacity() const { return m_opacity; }

	// PBR properties - Future
	void SetMetallic(float metallic);
	void SetRoughness(float roughness);
	void SetAO(float ao);

	const float& GetMetallic() const { return m_metallic; }
	const float& GetRoughness() const { return m_roughness; }
	const float& GetAO() const { return m_ao; }

	// Texture Managment
	void SetTexture(TextureType type, std::unique_ptr<TextureInfo> textureInfo);
	std::optional<std::reference_wrapper<TextureInfo>> GetTextureInfo(TextureType type) const;
	const std::unordered_map<TextureType, std::unique_ptr<TextureInfo>>& GetAllTextureInfo();
	bool HasTexture(TextureType type) const;
	void RemoveTexture(TextureType type);

	// Utility methods
	void SetName(const std::string& name);
	const std::string& GetName() const;

	// Apply material to shader
	void ApplyToShader(Shader& shader) const;

	// IAsset interface
	std::string CompileToResource(const std::string& assetPath) override;
	bool LoadResource(const std::string& assetPath) override;
	bool ReloadResource(const std::string& assetPath) override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) override;

	// Static factory methods for common materials
	static std::shared_ptr<Material> CreateDefault();
	static std::shared_ptr<Material> CreateMetal(const glm::vec3& color);
	static std::shared_ptr<Material> CreatePlastic(const glm::vec3& color);
	static std::shared_ptr<Material> CreateWood();

	void DebugPrintProperties() const;
private:
	std::string m_name;

	// Basic material properties
	glm::vec3 m_ambient{ 0.2f, 0.2f, 0.2f };
	glm::vec3 m_diffuse{ 0.8f, 0.8f, 0.8f };
	glm::vec3 m_specular{ 1.0f, 1.0f, 1.0f };
	glm::vec3 m_emissive{ 0.0f, 0.0f, 0.0f };
	float m_shininess{ 32.0f };
	float m_opacity{ 1.0f };

	// PBR properties
	float m_metallic{ 0.0f };
	float m_roughness{ 0.5f };
	float m_ao{ 1.0f };

	// Texture storage
	std::unordered_map<TextureType, std::unique_ptr<TextureInfo>> m_textureInfo;

	// Helper methods
	std::string TextureTypeToString(TextureType type) const;
	void BindTextures(Shader& shader) const;
	std::filesystem::path ResolveToProjectRoot(const std::filesystem::path& path);
};