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
#ifdef EDITOR
#include "compressonator.h"
#endif
#include "Utilities/FileUtilities.hpp"
#include <Asset Manager/AssetManager.hpp>
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

#include "Logging.hpp"
Texture::Texture() : ID(0), type(""), unit(-1), target(GL_TEXTURE_2D) {}

Texture::Texture(const char* texType, GLint slot, bool flipUVs) :
	ID(0), type(texType), unit(slot), target(GL_TEXTURE_2D), flipUVs(flipUVs) {}

std::string Texture::CompileToResource(const std::string& assetPath, bool forAndroid) {
	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;
	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load(flipUVs);
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
	bool needsAlpha = !forAndroid || numColCh > 3;
	stbi_image_free(bytes);
	bytes = stbi_load(assetPath.c_str(), &widthImg, &heightImg, &numColCh, needsAlpha ? 4 : 3);
#endif

	// Check if image loading failed
	if (!bytes) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Failed to load image: ", assetPath, "\n");
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: stbi_failure_reason: ", stbi_failure_reason(), "\n");
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Current working directory: ", std::filesystem::current_path(), "\n");
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: File exists check: ", std::filesystem::exists(assetPath), "\n");
		return std::string{}; // Return empty string to indicate failure
	}
	ENGINE_PRINT("[TEXTURE]: Successfully loaded image: " , assetPath , " (" , widthImg , "x" , heightImg , ", " , numColCh , " channels)\n");

	std::string outPath{};

#ifdef EDITOR
	// Compile the texture first.
	CMP_Texture srcTexture;
	srcTexture.dwSize = sizeof(srcTexture);
	srcTexture.dwWidth = widthImg;
	srcTexture.dwHeight = heightImg;
	srcTexture.dwPitch = 0;
	if (needsAlpha) {
		// force 4 channels
		// ensure you actually loaded with 4 channels from stbi_load(..., 4)
		srcTexture.format = CMP_FORMAT_RGBA_8888;
		srcTexture.dwDataSize = widthImg * heightImg * 4;
	}
	else {
		// prefer 3 channels to speed up ETC2 RGB
		// load with stbi_load(..., 3) or convert from 4 to 3
		srcTexture.format = CMP_FORMAT_RGB_888;
		srcTexture.dwDataSize = widthImg * heightImg * 3;
	}
	srcTexture.pData = bytes;

	CMP_Texture dstTexture;
	dstTexture.dwSize = sizeof(dstTexture);
	dstTexture.dwWidth = widthImg;
	dstTexture.dwHeight = heightImg;
	dstTexture.dwPitch = 0;
	dstTexture.format = forAndroid
		? (needsAlpha ? CMP_FORMAT_ETC2_RGBA : CMP_FORMAT_ETC2_RGB)
		: (type != "normal" ? CMP_FORMAT_BC3 : CMP_FORMAT_BC5);

	dstTexture.dwDataSize = CMP_CalculateBufferSize(&dstTexture);
	dstTexture.pData = (CMP_BYTE*)malloc(dstTexture.dwDataSize);

	// Set compression options.
	CMP_CompressOptions options = { 0 };
	options.dwSize = sizeof(options);

	// Compress the texture.
	ENGINE_PRINT("[Texture] Compressing texture: ", assetPath, "\n");
	CMP_ERROR cmp_status;
	cmp_status = CMP_ConvertTexture(&srcTexture, &dstTexture, &options, nullptr);
	if (cmp_status != CMP_OK) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Failed to compress texture.\n");
	}

	// Save the compresed texture to a DDS file.
	gli::format fmt = gli::FORMAT_UNDEFINED;
	if (!forAndroid) {
		fmt = type != "normal" ? gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16
			: gli::FORMAT_RG_ATI2N_UNORM_BLOCK16;
	}
	else {
		fmt = needsAlpha ? gli::FORMAT_RGBA_ETC2_UNORM_BLOCK16
			: gli::FORMAT_RGB_ETC2_UNORM_BLOCK8;
	}

	gli::extent2d extent(dstTexture.dwWidth, dstTexture.dwHeight);

	gli::texture2d tex(fmt, extent, 1); // 1 mip-map level
	if (dstTexture.pData != nullptr && dstTexture.dwDataSize > 0)
	{
		std::memcpy(tex.data(), dstTexture.pData, dstTexture.dwDataSize);
	}
	else
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: dstTexture.pData is null or dwDataSize is zero. Skipping memcpy.\n");
	}

	// Save the texture to a DDS file.
	std::filesystem::path p(assetPath);

	if (!forAndroid) {
		outPath = (p.parent_path() / p.stem()).generic_string() + ".dds";
		gli::save(tex, outPath);

		//// Save to the root project directory as well.
		//p = (FileUtilities::GetSolutionRootDir() / outPath);
		//gli::save(tex, p.generic_string());
	}
	else {
		std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
		assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
		outPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + "_android.ktx";
		// Ensure parent directories exist
		std::filesystem::path newPath(outPath);
		std::filesystem::create_directories(newPath.parent_path());
		gli::save(tex, outPath);
		
		// TEST CONVERT BACK TO PNG SEE IF IT'S CORRECT.
		//CMP_Texture dstTexture2;
		//dstTexture2.dwSize = sizeof(dstTexture2);
		//dstTexture2.dwWidth = dstTexture.dwWidth;
		//dstTexture2.dwHeight = dstTexture.dwHeight;
		//dstTexture2.dwPitch = 0;
		//dstTexture2.format = srcTexture.format;
		//dstTexture2.dwDataSize = dstTexture2.dwWidth * dstTexture2.dwHeight * 4;
		//dstTexture2.pData = (CMP_BYTE*)malloc(dstTexture2.dwDataSize);

		//// Decompress ETC2 - RGBA8
		//CMP_ERROR status = CMP_ConvertTexture(&dstTexture, &dstTexture2, &options, nullptr);
		//if (status != CMP_OK) {
		//	printf("Decompression failed: %d\n", status);
		//}

		//// Save as PNG (RGBA8 expected)
		//stbi_write_png((AssetManager::GetInstance().GetAndroidResourcesPath() / p.parent_path() / p.stem()).generic_string().c_str(),
		//	dstTexture2.dwWidth, dstTexture2.dwHeight,
		//	4,                // num channels
		//	dstTexture2.pData,
		//	dstTexture2.dwWidth * 4);    // stride in bytes
	}

	// Free original image data.
	stbi_image_free(bytes);
	free(dstTexture.pData);
#endif

	return outPath;
}

bool Texture::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
	ENGINE_LOG_DEBUG("[TEXTURE] Texture::LoadResource()");
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_LOG_DEBUG("[TEXTURE] ERROR: Platform not available for asset discovery!");
		return false;
	}

	std::filesystem::path resourcePathFS(resourcePath);

	// Load the meta file to get texture parameters
	std::string metaFilePath = assetPath + ".meta";
	if (!platform->FileExists(metaFilePath)) {
		ENGINE_LOG_DEBUG("[TEXTURE]: Meta file not found for texture: " + resourcePath);
		return false;
	}
	ENGINE_LOG_DEBUG("[TEXTURE] Meta file exists");
	std::vector<uint8_t> metaFileData = platform->ReadAsset(metaFilePath);
	rapidjson::Document doc;
	if (!metaFileData.empty()) {
		rapidjson::MemoryStream ms(reinterpret_cast<const char*>(metaFileData.data()), metaFileData.size());
		doc.ParseStream(ms);
	}
	if (doc.HasParseError()) {
		ENGINE_LOG_DEBUG("[TEXTURE]: Rapidjson parse error: " + metaFilePath);
	}
	//std::ifstream ifs(metaFilePath);
	//std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	//doc.Parse(jsonContent.c_str());

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
	if (!platform->FileExists(resourcePath)) {
		//std::cerr << "[TEXTURE] DEBUG: DDS file does not exist: " << path << std::endl;
		return false;
	}

	std::vector<uint8_t> texData = platform->ReadAsset(resourcePath);

	gli::texture texture = gli::load(reinterpret_cast<const char*>(texData.data()), texData.size());
	//std::cout << "[TEXTURE] DEBUG: GLI texture loaded, empty: " << texture.empty() << ", size: " << texture.size() << std::endl;

	if (texture.empty()) {
		//std::cerr << "[TEXTURE] DEBUG: GLI texture is empty!" << std::endl;
		return false;
	}

	//std::cout << "[TEXTURE] DEBUG: Texture dimensions: " << widthImg << "x" << heightImg << std::endl;
	
#ifndef ANDROID
	gli::gl GL(gli::gl::PROFILE_GL33);
#else
	gli::gl GL(gli::gl::PROFILE_ES30);
#endif
	gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());
	target = GL.translate(texture.target());

	// Generates an OpenGL texture object
	glGenTextures(1, &ID);
	glBindTexture(target, ID);
	glCompressedTexImage2D(
		target,
		0,
		format.Internal,
		static_cast<GLsizei>(texture.extent().x),
		static_cast<GLsizei>(texture.extent().y),
		0,
		static_cast<GLsizei>(texture.size()),
		texture.data()
	);

	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//// Generates MipMaps
	//glGenerateMipmap(target);

	// Unbinds the OpenGL Texture object so that it can't accidentally be modified
	glBindTexture(target, 0);

	//std::cout << "[TEXTURE] DEBUG: Texture loading completed successfully! Final ID: " << ID << std::endl;
	return true;
}

bool Texture::ReloadResource(const std::string& resourcePath, const std::string& assetPath) {
	return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Texture::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid) {
	std::string metaFilePath{};
	if (!forAndroid) {
		metaFilePath = assetPath + ".meta";
	}
	else {
		std::string assetPathAndroid = assetPath.substr(assetPath.find("Resources"));
		metaFilePath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + ".meta";
	}
	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());
	ifs.close();

	auto& allocator = doc.GetAllocator();

	rapidjson::Value textureMetaData(rapidjson::kObjectType);

	//// Add ID
	//textureMetaData.AddMember("id", rapidjson::Value().SetInt(static_cast<int>(ID)), allocator);
	//// Add unit
	//textureMetaData.AddMember("unit", rapidjson::Value().SetInt(static_cast<int>(unit)), allocator);
	// Add type
	textureMetaData.AddMember("type", rapidjson::Value().SetString(type.c_str(), allocator), allocator);

	doc.AddMember("TextureMetaData", textureMetaData, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	std::ofstream metaFile(metaFilePath);
	metaFile << buffer.GetString();
	metaFile.close();

	//if (!forAndroid) {
	//	// Save the meta file in the root project directory as well.
	//	try {
	//		std::filesystem::copy_file(metaFilePath, (FileUtilities::GetSolutionRootDir() / metaFilePath).generic_string(),
	//			std::filesystem::copy_options::overwrite_existing);
	//	}
	//	catch (const std::filesystem::filesystem_error& e) {
	//		std::cerr << "[Asset] Copy failed: " << e.what() << std::endl;
	//	}
	//}
	//else {
	//	// Save the meta file to the build and root directory as well.
	//	try {
	//		std::string buildMetaPath = assetPath + ".meta";
	//		std::filesystem::copy_file(metaFilePath, buildMetaPath,
	//			std::filesystem::copy_options::overwrite_existing);
	//		std::filesystem::copy_file(metaFilePath, (FileUtilities::GetSolutionRootDir() / buildMetaPath).generic_string(),
	//			std::filesystem::copy_options::overwrite_existing);
	//	}
	//	catch (const std::filesystem::filesystem_error& e) {
	//		std::cerr << "[Asset] Copy failed: " << e.what() << std::endl;
	//	}
	//}

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

void Texture::texUnit(Shader& shader, const char* uniform, GLuint tUnit)
{
	// Shader needs to be activated before changing the value of a uniform
	shader.Activate();
	// Sets the value of the uniform
	shader.setInt(uniform, tUnit);
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

std::string Texture::GetType() {
	return type;
}