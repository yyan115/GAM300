#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <string>
#include "Asset Manager/Asset.hpp"

struct Character {
	unsigned int textureID;
	glm::ivec2 size;
	glm::ivec2 Bearing;
	unsigned int advance;
};

class Font : public IAsset {
public:
	bool LoadAsset(const std::string& path) override;

private:
	std::map<GLchar, Character> Characters;
};