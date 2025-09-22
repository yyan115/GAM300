#pragma once
#include "OpenGL.h"
#include <Graphics/stb_image.h>
#include "ShaderClass.h"
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

class ENGINE_API Texture : public IAsset {
public:
	GLuint ID{};
	std::string type;
	GLint unit;
	GLenum target;

	Texture();
	ENGINE_API Texture(const char* texType, GLint slot);

	ENGINE_API std::string CompileToResource(const std::string& assetPath) override;
	bool LoadResource(const std::string& assetPath) override;
	ENGINE_API std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) override;

	GLenum GetFormatFromExtension(const std::string& filePath);

	// Assigns a texture unit to a texture
	void texUnit(Shader& shader, const char* uniform, GLuint unit);
	// Binds a texture
	void Bind(GLint runtimeUnit);
	// Unbinds a texture
	void Unbind(GLint runtimeUnit);
	// Deletes a texture
	void Delete();
};

struct TextureInfo {
	std::string filePath;
	std::shared_ptr<Texture> texture;

	TextureInfo() = default;
	TextureInfo(const std::string& path, std::shared_ptr<Texture> tex) : filePath(path), texture(tex) {}
};