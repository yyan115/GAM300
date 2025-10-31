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


class Material : public IAsset {
public:
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

	ENGINE_API Material();
	Material(const std::string& name);
	ENGINE_API Material(std::shared_ptr<AssetMeta> metaData);
	~Material() = default;

	Material(const Material&) = delete;
	Material& operator=(const Material&) = delete;

	// Basic Material Properties
	void ENGINE_API SetAmbient(const glm::vec3 ambient);
	void ENGINE_API SetDiffuse(const glm::vec3& diffuse);
	void ENGINE_API SetSpecular(const glm::vec3& specular);
	void SetEmissive(const glm::vec3& emissive);
	void ENGINE_API SetShininess(float shininess);
	void ENGINE_API SetOpacity(float opacity);

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
	void ENGINE_API SetTexture(TextureType type, std::unique_ptr<TextureInfo> textureInfo);
	ENGINE_API std::optional<std::reference_wrapper<TextureInfo>>  GetTextureInfo(TextureType type) const;
	ENGINE_API const std::unordered_map<TextureType, std::unique_ptr<TextureInfo>>&  GetAllTextureInfo();
	bool HasTexture(TextureType type) const;
	void ENGINE_API RemoveTexture(TextureType type);

	// Utility methods
	void ENGINE_API SetName(const std::string& name);
	ENGINE_API const std::string&  GetName() const;

	// Apply material to shader
	void ApplyToShader(Shader& shader) const;

	// IAsset interface
	bool GetMaterialPropertiesFromAsset(const std::string& assetPath);
	std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	ENGINE_API std::string CompileUpdatedAssetToResource(const std::string& assetPath, bool forAndroid = false);
	bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;

	// Static factory methods for common materials
	ENGINE_API static std::shared_ptr<Material> CreateDefault();
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