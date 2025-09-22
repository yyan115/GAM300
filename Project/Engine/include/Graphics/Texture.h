#pragma once
#include "OpenGL.h"
#include <Graphics/stb_image.h>
#include "ShaderClass.h"
#include "Asset Manager/Asset.hpp"

class ENGINE_API Texture : public IAsset {
public:
	GLuint ID{};
	const char* type;
	GLuint unit;

	Texture();
	Texture(const char* image, const char* texType, GLuint slot, GLenum pixelType);

	bool LoadAsset(const std::string& path) override;
	GLenum GetFormatFromExtension(const std::string& filePath);

	// Assigns a texture unit to a texture
	void texUnit(Shader& shader, const char* uniform, GLuint unit);
	// Binds a texture
	void Bind();
	// Unbinds a texture
	void Unbind();
	// Deletes a texture
	void Delete();
};