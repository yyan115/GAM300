#include "pch.h"

#include "Graphics/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Graphics/stb_image.h"
#include "GLI/gli.hpp"
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include "Logging.hpp"
Texture::Texture() : ID(0), type(""), unit(-1), target(GL_TEXTURE_2D) {}

Texture::Texture(const char* texType, GLint slot) :
	ID(0), type(texType), unit(slot), target(GL_TEXTURE_2D) {}

std::string Texture::CompileToResource(const std::string& assetPath) {
	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;
	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load(true);
	// Reads the image from a file and stores it in bytes
	unsigned char* bytes = stbi_load(assetPath.c_str(), &widthImg, &heightImg, &numColCh, 0);

	// Dynamically choose the appropriate format.
	gli::format gliFormat;
	switch (numColCh)
	{
		case 1:
			gliFormat = gli::FORMAT_R8_UNORM_PACK8;
			break;
		case 2:
			gliFormat = gli::FORMAT_RG8_UNORM_PACK8;
			break;
		case 3:
			gliFormat = gli::FORMAT_RGB8_UNORM_PACK8;
			break;
		case 4:
			gliFormat = gli::FORMAT_RGBA8_UNORM_PACK8;
			break;
		default:
			stbi_image_free(bytes);
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Unsupported number of color channels: ", numColCh, "\n");
			//std::cerr << "[TEXTURE]: Unsupported number of color channels: " << numColCh << std::endl;
			return std::string{}; // Unsupported format
	}

	// Create GLI texture object.
	gli::texture2d texture(gliFormat, glm::uvec2(widthImg, heightImg), 1);
	std::memcpy(texture.data(), bytes, widthImg * heightImg * numColCh);

	// Save the texture to a DDS file.
	std::filesystem::path p(assetPath);
	std::string outPath = (p.parent_path() / p.stem()).generic_string() + ".dds";
	gli::save(texture, outPath);

	// Free original image data.
	stbi_image_free(bytes);

	return outPath;
}

bool Texture::LoadResource(const std::string& assetPath) {
	std::filesystem::path assetPathFS(assetPath);

	// Load the meta file to get texture parameters
	std::string metaFilePath = assetPathFS.string() + ".meta";
	if (!std::filesystem::exists(metaFilePath)) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Meta file not found for texture: ", assetPath, "\n");
		//std::cerr << "[TEXTURE]: Meta file not found for texture: " << assetPath << std::endl;
		return false;
	}

	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());

	const auto& textureMetaData = doc["TextureMetaData"];

	//if (textureMetaData.HasMember("id")) {
	//	ID = static_cast<GLuint>(textureMetaData["id"].GetInt());
	//}
	
	if (textureMetaData.HasMember("type")) {
		type = textureMetaData["type"].GetString();
	}

	if (textureMetaData.HasMember("unit")) {
		unit = static_cast<GLuint>(textureMetaData["unit"].GetInt());
	}

	// Load the DDS file using GLI
	std::string path = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".dds";

	gli::texture texture = gli::load(path);
	void* bytes = texture.data();
	int widthImg = texture.extent().x;
	int heightImg = texture.extent().y;
	
	gli::gl GL(gli::gl::PROFILE_GL33);
	gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());
	target = GL.translate(texture.target());

	// Generates an OpenGL texture object
	glGenTextures(1, &ID);

	//// Assigns the texture to a Texture Unit
	//glActiveTexture(GL_TEXTURE0 + slot);
	//unit = slot;
	//glBindTexture(GL_TEXTURE_2D, ID);

	// Only bind to unit if slot is valid
	//if (unit >= 0)
	//{
	//	glActiveTexture(GL_TEXTURE0 + unit);
	//}
	glBindTexture(target, ID);

	// Configures the type of algorithm that is used to make the image smaller or bigger
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Configures the way the texture repeats (if it does at all)
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
	glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, &format.Swizzles[0]);

	// Extra lines in case you choose to use GL_CLAMP_TO_BORDER
	// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	// glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);

	// Assigns the image to the OpenGL Texture object
	if (target == GL_TEXTURE_2D) {
		glTexImage2D(target, 0, format.Internal, widthImg, heightImg, 0, format.External, format.Type, bytes);
	}
	else {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Unsupported texture target: ", target, "\n");
		//std::cerr << "[TEXTURE]: Unsupported texture target: " << target << std::endl;
		return false;
	}

	// Generates MipMaps
	glGenerateMipmap(target);

	// Unbinds the OpenGL Texture object so that it can't accidentally be modified
	glBindTexture(target, 0);

	return true;
}

std::shared_ptr<AssetMeta> Texture::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) {
	std::string metaFilePath = assetPath + ".meta";
	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());
	ifs.close();

	auto& allocator = doc.GetAllocator();

	rapidjson::Value textureMetaData(rapidjson::kObjectType);

	// Add ID
	textureMetaData.AddMember("id", rapidjson::Value().SetInt(static_cast<int>(ID)), allocator);
	// Add type
	textureMetaData.AddMember("type", rapidjson::Value().SetString(type.c_str(), allocator), allocator);
	// Add unit
	textureMetaData.AddMember("unit", rapidjson::Value().SetInt(static_cast<int>(unit)), allocator);

	doc.AddMember("TextureMetaData", textureMetaData, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	std::ofstream metaFile(metaFilePath);
	metaFile << buffer.GetString();
	metaFile.close();

	std::shared_ptr<TextureMeta> metaData = std::make_shared<TextureMeta>();
	metaData->PopulateAssetMeta(currentMetaData->guid, currentMetaData->sourceFilePath, currentMetaData->compiledFilePath, currentMetaData->version);
	metaData->PopulateTextureMeta(ID, type, unit);
	return metaData;
}

GLenum Texture::GetFormatFromExtension(const std::string& filepath) {
	std::string extension = filepath.substr(filepath.find_last_of('.'));

	if (extension == ".png" || extension == ".PNG")
	{
		return GL_RGBA;
	}
	else if (extension == ".jpg" || extension == ".jpeg" || extension == ".JPG" || extension == ".JPEG")
	{
		return GL_RGB;
	}
	else if (extension == ".bmp" || extension == ".BMP")
	{
		return GL_RGB;
	}
	else
	{
		// Default to RGB for unknown formats
		return GL_RGB;
	}
}

void Texture::texUnit(Shader& shader, const char* uniform, GLuint unit)
{
	// Shader needs to be activated before changing the value of a uniform
	shader.Activate();
	// Sets the value of the uniform
	shader.setInt(uniform, unit);
}

void Texture::Bind(GLint runtimeUnit)
{
	GLint unitToUse = (runtimeUnit >= 0) ? runtimeUnit : (unit >= 0 ? unit : 0);
	glActiveTexture(GL_TEXTURE0 + unitToUse);
	glBindTexture(target, ID);
}

void Texture::Unbind(GLint runtimeUnit)
{
	GLint unitToUse = (runtimeUnit >= 0) ? runtimeUnit : (unit >= 0 ? unit : 0);
	glActiveTexture(GL_TEXTURE0 + unitToUse);
	glBindTexture(target, 0);
}

void Texture::Delete()
{
	glDeleteTextures(1, &ID);
}