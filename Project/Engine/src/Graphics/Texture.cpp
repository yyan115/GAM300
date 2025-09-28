#include "pch.h"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/log.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif

#include "Graphics/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Graphics/stb_image.h"
#ifdef ANDROID
#include <GLI/gli.hpp>
#else
#include <gli/gli.hpp>
#endif
#include <filesystem>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

Texture::Texture() : ID(0), type(""), unit(-1), target(GL_TEXTURE_2D) {}

Texture::Texture(const char* texType, GLint slot) :
	ID(0), type(texType), unit(slot), target(GL_TEXTURE_2D) {}

std::string Texture::CompileToResource(const std::string& assetPath) {
	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;
	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load(true);
	// Reads the image from a file and stores it in bytes
	unsigned char* bytes = nullptr;

#ifdef __ANDROID__
	// On Android, load texture from AssetManager
	auto* platform = WindowManager::GetPlatform();
	if (platform) {
		AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
		AAssetManager* assetManager = androidPlatform->GetAssetManager();

		if (assetManager) {
			AAsset* asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
			if (asset) {
				off_t assetLength = AAsset_getLength(asset);
				const unsigned char* assetData = (const unsigned char*)AAsset_getBuffer(asset);

				if (assetData && assetLength > 0) {
					bytes = stbi_load_from_memory(assetData, (int)assetLength, &widthImg, &heightImg, &numColCh, 0);
					__android_log_print(ANDROID_LOG_INFO, "GAM300", "[TEXTURE] Loaded texture from Android assets: %s (%dx%d, %d channels)", assetPath.c_str(), widthImg, heightImg, numColCh);
				} else {
					__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[TEXTURE] Failed to get asset data for: %s", assetPath.c_str());
				}
				AAsset_close(asset);
			} else {
				__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[TEXTURE] Failed to open asset: %s", assetPath.c_str());
			}
		} else {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[TEXTURE] AssetManager not available");
		}
	} else {
		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[TEXTURE] Platform not available");
	}
#else
	// On other platforms, load from filesystem
	bytes = stbi_load(assetPath.c_str(), &widthImg, &heightImg, &numColCh, 0);
#endif

	// Check if image loading failed
	if (!bytes) {
		std::cerr << "[TEXTURE]: Failed to load image: " << assetPath << std::endl;
		std::cerr << "[TEXTURE]: stbi_failure_reason: " << stbi_failure_reason() << std::endl;
		std::cerr << "[TEXTURE]: Current working directory: " << std::filesystem::current_path() << std::endl;
		std::cerr << "[TEXTURE]: File exists check: " << std::filesystem::exists(assetPath) << std::endl;
		return std::string{}; // Return empty string to indicate failure
	}

	std::cout << "[TEXTURE]: Successfully loaded image: " << assetPath << " (" << widthImg << "x" << heightImg << ", " << numColCh << " channels)" << std::endl;

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
			std::cerr << "[TEXTURE]: Unsupported number of color channels: " << numColCh << std::endl;
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
	//std::cout << "[TEXTURE] DEBUG: Loading texture resource: " << assetPath << std::endl;
	std::filesystem::path assetPathFS(assetPath);

	// Load the meta file to get texture parameters
	std::string metaFilePath = assetPathFS.string() + ".meta";
	//std::cout << "[TEXTURE] DEBUG: Looking for meta file: " << metaFilePath << std::endl;
	if (!std::filesystem::exists(metaFilePath)) {
		std::cerr << "[TEXTURE]: Meta file not found for texture: " << assetPath << std::endl;
		return false;
	}
	//std::cout << "[TEXTURE] DEBUG: Meta file found, loading..." << std::endl;

	std::ifstream ifs(metaFilePath);
	if (!ifs.is_open()) {
		std::cerr << "[TEXTURE]: Failed to open meta file: " << metaFilePath << std::endl;
		return false;
	}

	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();

	if (jsonContent.empty()) {
		std::cerr << "[TEXTURE]: Meta file is empty: " << metaFilePath << std::endl;
		return false;
	}

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());

	if (doc.HasParseError()) {
		std::cerr << "[TEXTURE]: Failed to parse JSON in meta file: " << metaFilePath << std::endl;
		return false;
	}

	if (!doc.IsObject() || !doc.HasMember("TextureMetaData") || !doc["TextureMetaData"].IsObject()) {
		std::cerr << "[TEXTURE]: Invalid JSON structure in meta file: " << metaFilePath << std::endl;
		return false;
	}

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
	//std::cout << "[TEXTURE] DEBUG: Loading DDS file: " << path << std::endl;

	if (!std::filesystem::exists(path)) {
		//std::cerr << "[TEXTURE] DEBUG: DDS file does not exist: " << path << std::endl;
		return false;
	}

	gli::texture texture = gli::load(path);
	//std::cout << "[TEXTURE] DEBUG: GLI texture loaded, empty: " << texture.empty() << ", size: " << texture.size() << std::endl;

	if (texture.empty()) {
		//std::cerr << "[TEXTURE] DEBUG: GLI texture is empty!" << std::endl;
		return false;
	}

	void* bytes = texture.data();
	int widthImg = texture.extent().x;
	int heightImg = texture.extent().y;
	//std::cout << "[TEXTURE] DEBUG: Texture dimensions: " << widthImg << "x" << heightImg << std::endl;
	
	gli::gl GL(gli::gl::PROFILE_GL33);
	gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());
	target = GL.translate(texture.target());

	// Generates an OpenGL texture object
	glGenTextures(1, &ID);
	//std::cout << "[TEXTURE] DEBUG: Generated OpenGL texture ID: " << ID << std::endl;

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
	//std::cout << "[TEXTURE] DEBUG: Bound texture to target: " << target << std::endl;

	// Configures the type of algorithm that is used to make the image smaller or bigger
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Configures the way the texture repeats (if it does at all)
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
#ifndef ANDROID
	glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, &format.Swizzles[0]);
#endif

	// Extra lines in case you choose to use GL_CLAMP_TO_BORDER
	// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	// glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);

	// Assigns the image to the OpenGL Texture object
	if (target == GL_TEXTURE_2D) {
		glTexImage2D(target, 0, format.Internal, widthImg, heightImg, 0, format.External, format.Type, bytes);
	}
	else {
		std::cerr << "[TEXTURE]: Unsupported texture target: " << target << std::endl;
		return false;
	}

	// Generates MipMaps
	glGenerateMipmap(target);

	// Unbinds the OpenGL Texture object so that it can't accidentally be modified
	glBindTexture(target, 0);

	//std::cout << "[TEXTURE] DEBUG: Texture loading completed successfully! Final ID: " << ID << std::endl;
	return true;
}

bool Texture::ReloadResource(const std::string& assetPath) {
	// Load the DDS file using GLI
	std::filesystem::path assetPathFS(assetPath);
	std::string path = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".dds";

	gli::texture texture = gli::load(path);
	void* bytes = texture.data();
	int widthImg = texture.extent().x;
	int heightImg = texture.extent().y;

	gli::gl GL(gli::gl::PROFILE_GL33);
	gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());
	target = GL.translate(texture.target());

	glBindTexture(target, ID);

	// Configures the type of algorithm that is used to make the image smaller or bigger
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Configures the way the texture repeats (if it does at all)
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
#ifndef ANDROID
	glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, &format.Swizzles[0]);
#endif

	// Extra lines in case you choose to use GL_CLAMP_TO_BORDER
	// float flatColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	// glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, flatColor);

	// Assigns the image to the OpenGL Texture object
	if (target == GL_TEXTURE_2D) {
		glTexImage2D(target, 0, format.Internal, widthImg, heightImg, 0, format.External, format.Type, bytes);
	}
	else {
		std::cerr << "[TEXTURE]: Unsupported texture target: " << target << std::endl;
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
	//std::cout << "[TEXTURE DEBUG] Rendering with texture ID: " << ID << std::endl;
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