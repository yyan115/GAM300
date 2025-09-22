#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include "OpenGL.h"

// Forward declaration instead of full include
class Texture;

class TextureManager {
public:
	static TextureManager& getInstance();

	std::shared_ptr<Texture> loadTexture(const std::string& filepath, const std::string& type = "diffuse");

	std::shared_ptr<Texture> getTexture(const std::string& filepath);

	bool isLoaded(const std::string& filepath) const;

	void unloadTexture(const std::string& filepath);

	void clearCache();

	size_t getCacheSize() const;
	void printCacheStats() const;

	// Prevent copying
	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

private:
	TextureManager() = default;
	~TextureManager() = default;

	std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache;
	static int nextTextureUnit;

	GLenum getFormatFromExtension(const std::string& filepath);
};