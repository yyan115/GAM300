#pragma once
#include "Graphics/OpenGL.h"
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <array>
#include <cstddef>
#include <string>
#include "Asset Manager/Asset.hpp"

class VAO;
class VBO;

struct Character {
	unsigned int textureID = 0;
	glm::ivec2 size{0};
	glm::ivec2 bearing{0};
	unsigned int advance = 0;
	glm::vec2 uvMin{0.0f};
	glm::vec2 uvMax{0.0f};
};

class ENGINE_API Font : public IAsset {
public:
	Font(unsigned int fontSize = 48);
	Font(std::shared_ptr<AssetMeta> fontMeta, unsigned int fontSize = 48);
	~Font();

	std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
	void Cleanup();
	//bool LoadFont(const std::string& path, unsigned int fontSize);
	bool LoadResource(const std::string& resourcePath, const std::string& assetPath, unsigned int newFontSize, bool setFontSize = true);
	bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
	std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid = false) override;

	void SetFontSize(unsigned int newSize);
	unsigned int GetFontSize() const { return fontSize; }
	const Character& GetCharacter(char c) const;
	float GetTextWidth(const std::string& text, float scale = 1.0f) const;
	float GetTextHeight(float scale = 1.0f) const;

	VAO* GetVAO() const { return textVAO.get(); }
	VBO* GetVBO() const { return textVBO.get(); }
	unsigned int GetAtlasTexture() const { return atlasTexture; }
	void EnsureTextVertexCapacity(std::size_t vertexCount);
private:
	std::array<Character, 128> Characters{};
	std::unique_ptr<VAO> textVAO;
	std::unique_ptr<VBO> textVBO;
	unsigned int atlasTexture = 0;
	std::size_t textVertexCapacity = 0;
	int maxGlyphHeight = 0;
	unsigned int fontSize;
	std::string fontAssetPath;
	std::string fontResourcePath;
	std::vector<uint8_t> buffer;
};
