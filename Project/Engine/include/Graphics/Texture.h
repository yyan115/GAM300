#pragma once
#include "OpenGL.h"
#include <Graphics/stb_image.h>
#include "ShaderClass.h"
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

class Texture : public IAsset {
public:

	GLuint ID{};
	//std::string type;
	GLint unit;
	GLenum target;

	// Asset browser preview thumbnail (for normal maps)
	GLuint previewID{};

	ENGINE_API Texture();
	Texture(const char* texType, GLint slot, bool flipUVs = false, bool generateMipmaps = true);
	Texture(std::shared_ptr<TextureMeta> textureMeta);

	std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;

	GLenum GetFormatFromExtension(const std::string& filePath);

	// Assigns a texture unit to a texture
	void texUnit(Shader& shader, const char* uniform, GLuint tUnit);
	// Binds a texture
	void Bind(GLint runtimeUnit);
	// Unbinds a texture
	void Unbind(GLint runtimeUnit);
	// Deletes a texture
	void Delete();

	ENGINE_API std::string GetType();

	// Asset browser preview thumbnail (for normal maps)
	void ReconstructBC5Preview(
		const uint8_t* rgbaTexData, int texWidth, int texHeight);

private:
	//GLuint ID{};
	std::string type;
	bool flipUVs;
	bool generateMipmaps;
	//GLint unit;
	//GLenum target;
};

struct TextureInfo {
	std::string filePath;
	std::shared_ptr<Texture> texture;

	TextureInfo() = default;
	TextureInfo(const std::string& path, std::shared_ptr<Texture> tex) : filePath(path), texture(tex) {}
};