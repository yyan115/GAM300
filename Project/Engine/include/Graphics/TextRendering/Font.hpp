#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <string>
#include "Asset Manager/Asset.hpp"

class VAO;
class VBO;

struct Character {
	unsigned int textureID;
	glm::ivec2 size;
	glm::ivec2 Bearing;
	unsigned int advance;
};

class Font : public IAsset {
public:
	Font(unsigned int defaultFontSize = 48);
	~Font();

	bool LoadAsset(const std::string& path) override;
	void Cleanup();
	bool LoadFont(const std::string& path, unsigned int fontSize);

	void SetFontSize(unsigned int newSize);
	unsigned int GetFontSize() const { return fontSize; }
	const Character& GetCharacter(char c) const;
	float GetTextWidth(const std::string& text, float scale = 1.0f) const;
	float GetTextHeight(float scale = 1.0f) const;

	VAO* GetVAO() const { return textVAO.get(); }
	VBO* GetVBO() const { return textVBO.get(); }
private:
	std::map<GLchar, Character> Characters;
	std::unique_ptr<VAO> textVAO;
	std::unique_ptr<VBO> textVBO;
	unsigned int fontSize;
	std::string fontPath;
};