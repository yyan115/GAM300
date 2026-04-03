#include "pch.h"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/log.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif

#include "Graphics/Texture.h"

// STB Image implementation - header-only library needs implementation defined once
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

#ifdef EDITOR
// Box-filter a pixel buffer down to the next mip level (2x downsample).
static std::vector<uint8_t> BoxFilterMip(const uint8_t* src, int srcW, int srcH, int channels) {
	int dstW = std::max(1, srcW / 2);
	int dstH = std::max(1, srcH / 2);
	std::vector<uint8_t> dst(dstW * dstH * channels);
	for (int y = 0; y < dstH; ++y) {
		for (int x = 0; x < dstW; ++x) {
			int x0 = x * 2, y0 = y * 2;
			int x1 = std::min(x0 + 1, srcW - 1);
			int y1 = std::min(y0 + 1, srcH - 1);
			for (int c = 0; c < channels; ++c) {
				int sum = (int)src[(y0 * srcW + x0) * channels + c]
					+ (int)src[(y0 * srcW + x1) * channels + c]
					+ (int)src[(y1 * srcW + x0) * channels + c]
					+ (int)src[(y1 * srcW + x1) * channels + c];
				dst[(y * dstW + x) * channels + c] = (uint8_t)(sum >> 2);
			}
		}
	}
	return dst;
}

// Compress a single raw pixel buffer to block format and copy into a gli mip level.
static bool CompressAndStoreMip(
	const uint8_t* pixels, int w, int h, int channels,
	CMP_FORMAT srcFmt, CMP_FORMAT dstFmt,
	const CMP_CompressOptions& options,
	gli::texture2d& tex, int mipLevel)
{
	CMP_Texture mipSrc = {};
	mipSrc.dwSize = sizeof(mipSrc);
	mipSrc.dwWidth = (CMP_DWORD)w;
	mipSrc.dwHeight = (CMP_DWORD)h;
	mipSrc.dwPitch = 0;
	mipSrc.format = srcFmt;
	mipSrc.dwDataSize = (CMP_DWORD)(w * h * channels);
	mipSrc.pData = const_cast<CMP_BYTE*>(pixels);

	CMP_Texture mipDst = {};
	mipDst.dwSize = sizeof(mipDst);
	mipDst.dwWidth = (CMP_DWORD)w;
	mipDst.dwHeight = (CMP_DWORD)h;
	mipDst.dwPitch = 0;
	mipDst.format = dstFmt;
	mipDst.dwDataSize = CMP_CalculateBufferSize(&mipDst);
	mipDst.pData = (CMP_BYTE*)malloc(mipDst.dwDataSize);

	CMP_ERROR status = CMP_ConvertTexture(&mipSrc, &mipDst, &options, nullptr);
	if (status != CMP_OK) {
		free(mipDst.pData);
		return false;
	}

	// GLI and Compressonator must agree on block-aligned sizes.
	std::size_t gliSize = tex.size(mipLevel);
	std::memcpy(tex.data(0, 0, mipLevel), mipDst.pData, std::min((std::size_t)mipDst.dwDataSize, gliSize));
	free(mipDst.pData);
	return true;
}
#endif
Texture::Texture() : ID(0), unit(-1), target(GL_TEXTURE_2D) {
	metaData = std::make_shared<TextureMeta>();
}

//Texture::Texture(const char* texType, GLint slot, bool flipUVs, bool generateMipmaps) :
//	ID(0), type(texType), unit(slot), target(GL_TEXTURE_2D), flipUVs(flipUVs), generateMipmaps(generateMipmaps) {}

Texture::Texture(std::shared_ptr<TextureMeta> textureMeta) :
	ID(0), unit(-1), target(GL_TEXTURE_2D), metaData(textureMeta) {}

std::string Texture::CompileToResource(const std::string& assetPath, bool forAndroid) {
	// Stores the width, height, and the number of color channels of the image
	int widthImg, heightImg, numColCh;
	// Flips the image so it appears right side up
	stbi_set_flip_vertically_on_load_thread(metaData->flipUVs);
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
	//ENGINE_PRINT("[TEXTURE]: Successfully loaded image: " , assetPath , " (" , widthImg , "x" , heightImg , ", " , numColCh , " channels)\n");

	std::string outPath{};

#ifdef EDITOR
	// Downscale to maxSize if either dimension exceeds it.
	// Uses successive 2x box-filtering (same as mip generation) so quality is consistent.
	if (metaData->maxSize > 0 && (widthImg > metaData->maxSize || heightImg > metaData->maxSize)) {
		int srcChannels = needsAlpha ? 4 : 3;
		std::vector<uint8_t> pixels(bytes, bytes + (std::size_t)(widthImg * heightImg * srcChannels));
		while (widthImg > metaData->maxSize || heightImg > metaData->maxSize) {
			pixels = BoxFilterMip(pixels.data(), widthImg, heightImg, srcChannels);
			widthImg = std::max(1, widthImg / 2);
			heightImg = std::max(1, heightImg / 2);
		}
		stbi_image_free(bytes);
		bytes = (unsigned char*)malloc(pixels.size());
		std::memcpy(bytes, pixels.data(), pixels.size());
		//ENGINE_PRINT("[TEXTURE]: Downscaled to maxSize ", metaData->maxSize, ": ", widthImg, "x", heightImg, "\n");
	}

	// Number of channels actually present in the loaded pixel data.
	int srcChannels = needsAlpha ? 4 : 3;
	CMP_FORMAT srcCmpFmt = needsAlpha ? CMP_FORMAT_RGBA_8888 : CMP_FORMAT_RGB_888;

	// Choose the best output block format.
	//   PC  : BC5 for normal maps, BC3 for textures with real alpha, BC1 for everything else.
	//         BC1 is half the size of BC3 (8 vs 16 bytes per 4x4 block) and perfectly fine
	//         for opaque diffuse / roughness / metallic / AO / height maps.
	//   Android: ETC2_RGBA when alpha present, ETC2_RGB otherwise.
	CMP_FORMAT dstCmpFmt;
	gli::format gliFormat;
	if (forAndroid) {
		dstCmpFmt = needsAlpha ? CMP_FORMAT_ETC2_RGBA : CMP_FORMAT_ETC2_RGB;
		gliFormat = needsAlpha ? gli::FORMAT_RGBA_ETC2_UNORM_BLOCK16 : gli::FORMAT_RGB_ETC2_UNORM_BLOCK8;
	}
	else if (metaData->type == "normal") {
		dstCmpFmt = CMP_FORMAT_BC5;
		gliFormat = gli::FORMAT_RG_ATI2N_UNORM_BLOCK16;
	}
	else if (numColCh > 3) {
		// Source file genuinely has an alpha channel — keep it.
		dstCmpFmt = CMP_FORMAT_BC3;
		gliFormat = gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16;
	}
	else {
		// Opaque texture — BC1 uses half the GPU memory of BC3.
		dstCmpFmt = CMP_FORMAT_BC1;
		gliFormat = gli::FORMAT_RGB_DXT1_UNORM_BLOCK8;
	}

	// Full mip chain: floor(log2(max_dim)) + 1 levels.
	int mipCount = 1 + (int)std::floor(std::log2((float)std::max(widthImg, heightImg)));

	gli::extent2d extent(widthImg, heightImg);
	gli::texture2d tex(gliFormat, extent, (gli::texture2d::size_type)mipCount);

	CMP_CompressOptions options = { 0 };
	options.dwSize = sizeof(options);

	//ENGINE_PRINT("[Texture] Compressing texture (", mipCount, " mip levels): ", assetPath, "\n");

	// Compress mip 0 directly from the stbi-loaded pixels.
	if (!CompressAndStoreMip(bytes, widthImg, heightImg, srcChannels, srcCmpFmt, dstCmpFmt, options, tex, 0)) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE]: Failed to compress mip 0 for: ", assetPath, "\n");
		stbi_image_free(bytes);
		return {};
	}

	// Generate and compress the remaining mip levels by successive 2x box-filtering.
	std::vector<uint8_t> prevPixels(bytes, bytes + (std::size_t)(widthImg * heightImg * srcChannels));
	int mipW = widthImg, mipH = heightImg;
	for (int mip = 1; mip < mipCount; ++mip) {
		std::vector<uint8_t> mipPixels = BoxFilterMip(prevPixels.data(), mipW, mipH, srcChannels);
		mipW = std::max(1, mipW / 2);
		mipH = std::max(1, mipH / 2);
		if (!CompressAndStoreMip(mipPixels.data(), mipW, mipH, srcChannels, srcCmpFmt, dstCmpFmt, options, tex, mip)) {
			ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[TEXTURE]: Failed to compress mip ", mip, " - stopping mip chain early.\n");
			break;
		}
		prevPixels = std::move(mipPixels);
	}

	stbi_image_free(bytes);

	// Save the texture to a DDS (PC) or KTX (Android) file.
	std::filesystem::path p(assetPath);

	if (!forAndroid) {
		outPath = (p.parent_path() / p.stem()).generic_string() + ".dds";
		if (!gli::save(tex, outPath)) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[TEXTURE] FATAL: gli::save failed to write to ", outPath, "\n");
			outPath = "";
		}
		else {
			//ENGINE_PRINT(EngineLogging::LogLevel::Info, "[TEXTURE] Successfully compiled and saved: ", outPath, "\n");
		}
	}
	else {
		std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
		assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
		outPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + "_android.ktx";
		std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(outPath));
		outPath = newPath.generic_string();
		std::filesystem::create_directories(newPath.parent_path());
		gli::save(tex, outPath);
	}
#endif

	return outPath;
}

bool Texture::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
	//ENGINE_LOG_DEBUG("[TEXTURE] Texture::LoadResource()");
	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_LOG_DEBUG("[TEXTURE] ERROR: Platform not available for asset discovery!");
		return false;
	}

	std::filesystem::path resourcePathFS(resourcePath);

	// Load meta data from Asset Manager to get texture settings.
	GUID_128 guid = AssetManager::GetInstance().GetGUID128FromAssetMeta(assetPath);
	metaData = dynamic_pointer_cast<TextureMeta>(AssetManager::GetInstance().GetAssetMeta(guid));

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

	if (ID == 0) {
		GLenum error = glGetError();
		std::cerr << "[Texture] glGenTextures failed! OpenGL Error: " << error << std::endl;

#ifdef ANDROID
		__android_log_print(ANDROID_LOG_ERROR, "GAM300",
			"[Texture] glGenTextures failed! Error: %d, ID: %u", error, ID);
#endif

		// Check if context is current
#ifdef ANDROID
		EGLContext ctx = eglGetCurrentContext();
		if (ctx == EGL_NO_CONTEXT) {
			__android_log_print(ANDROID_LOG_ERROR, "GAM300",
				"[Texture] NO OPENGL CONTEXT CURRENT!");
		}
#endif
	}

	glBindTexture(target, ID);

	// Upload every mip level that was baked into the DDS/KTX at compile time.
	for (std::size_t level = 0; level < texture.levels(); ++level) {
		glCompressedTexImage2D(
			target,
			static_cast<GLint>(level),
			format.Internal,
			static_cast<GLsizei>(texture.extent(level).x),
			static_cast<GLsizei>(texture.extent(level).y),
			0,
			static_cast<GLsizei>(texture.size(level)),
			texture.data(0, 0, level)
		);
	}

	// Set texture wrapping mode.
	switch (metaData->textureWrapMode) {
		case TextureMeta::TextureWrapMode::Clamp: {
			// S = X axis, T = Y axis
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // mode should be GL_CLAMP/REPEAT
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			break;
		}
		
		case TextureMeta::TextureWrapMode::Repeat: {
			// S = X axis, T = Y axis
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // mode should be GL_CLAMP/REPEAT
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			break;
		}
	}

	// Use the mip chain that was baked into the file at compile time.
	// glGenerateMipmap() on a block-compressed texture is undefined in the OpenGL spec
	// and silently fails on most drivers, so we never call it here.
	if (metaData->generateMipmaps && texture.levels() > 1) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Unbinds the OpenGL Texture object so that it can't accidentally be modified
	glBindTexture(target, 0);

#ifdef EDITOR
	// Reconstruct the BC5 preview for the asset browser if it's a normal map
	if (metaData->type == "normal") {
		// Decompress BC5 to RGBA
		CMP_Texture srcTexture = {};
		srcTexture.dwSize = sizeof(CMP_Texture);
		srcTexture.dwWidth = texture.extent().x;
		srcTexture.dwHeight = texture.extent().y;
		srcTexture.format = CMP_FORMAT_BC5;
		srcTexture.dwDataSize = static_cast<CMP_DWORD>(texture.size(0)); // mip 0 only
		srcTexture.pData = reinterpret_cast<CMP_BYTE*>(texture.data(0, 0, 0));

		CMP_Texture dstTexture = {};
		dstTexture.dwSize = sizeof(CMP_Texture);
		dstTexture.dwWidth = srcTexture.dwWidth;
		dstTexture.dwHeight = srcTexture.dwHeight;
		dstTexture.format = CMP_FORMAT_RGBA_8888;  // Use RGBA instead of RG
		dstTexture.dwDataSize = CMP_CalculateBufferSize(&dstTexture);
		dstTexture.pData = (CMP_BYTE*)malloc(dstTexture.dwDataSize);

		CMP_CompressOptions options = {};
		options.dwSize = sizeof(options);

		CMP_ERROR status = CMP_ConvertTexture(&srcTexture, &dstTexture, &options, nullptr);
		if (status == CMP_OK) {
			ReconstructBC5Preview(dstTexture.pData, dstTexture.dwWidth, dstTexture.dwHeight);
			free(dstTexture.pData);
		}
	}
#endif

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
		std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(metaFilePath));
		metaFilePath = newPath.generic_string();
	}
	std::ifstream ifs(metaFilePath);
	std::string jsonContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

	rapidjson::Document doc;
	doc.Parse(jsonContent.c_str());
	ifs.close();

	auto& allocator = doc.GetAllocator();

	// Remove existing TextureMetaData if it exists to avoid double members
	if (doc.HasMember("TextureMetaData")) {
		doc.RemoveMember("TextureMetaData");
	}

	rapidjson::Value textureMetaData(rapidjson::kObjectType);

	// Add type
	textureMetaData.AddMember("type", rapidjson::Value().SetString(metaData->type.c_str(), allocator), allocator);
	// Add flip UVs
	textureMetaData.AddMember("flipUVs", rapidjson::Value().SetBool(metaData->flipUVs), allocator);
	// Add generate mipmaps
	textureMetaData.AddMember("generateMipmaps", rapidjson::Value().SetBool(metaData->generateMipmaps), allocator);
	// Add texture wrap mode
	textureMetaData.AddMember("textureWrapMode", rapidjson::Value().SetString(metaData->textureWrapModeStr.c_str(), allocator), allocator);
	// Add max size
	textureMetaData.AddMember("maxSize", rapidjson::Value().SetInt(metaData->maxSize), allocator);

	doc.AddMember("TextureMetaData", textureMetaData, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);

	std::ofstream metaFile(metaFilePath);
	metaFile << buffer.GetString();
	metaFile.close();

	std::shared_ptr<TextureMeta> newMetaData = std::make_shared<TextureMeta>();
	newMetaData->PopulateAssetMeta(currentMetaData->guid, currentMetaData->sourceFilePath, currentMetaData->compiledFilePath, currentMetaData->version);
	newMetaData->PopulateTextureMeta(metaData->type, metaData->flipUVs, metaData->generateMipmaps, metaData->textureWrapMode, metaData->maxSize);
	return newMetaData;
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
	return metaData->type;
}

void Texture::ReconstructBC5Preview(const uint8_t* rgbaTexData, int texWidth, int texHeight) {
	std::vector<uint8_t> outRGBA;
	outRGBA.resize(texWidth * texHeight * 4);

	for (int i = 0; i < texWidth * texHeight; i++) {
		// BC5 stores normals in RG channels after decompression
		// The values are in [0,255] representing normalized range
		uint8_t rByte = rgbaTexData[i * 4 + 0];
		uint8_t gByte = rgbaTexData[i * 4 + 1];

		// Convert [0,255] to [-1,1]
		// Standard encoding: 128 = 0.0, 0 = -1.0, 255 = +1.0
		float nx = (rByte - 128.0f) / 127.0f;
		float ny = (gByte - 128.0f) / 127.0f;

		// Clamp to [-1, 1]
		nx = std::max(-1.0f, std::min(1.0f, nx));
		ny = std::max(-1.0f, std::min(1.0f, ny));

		// Reconstruct Z (always positive for standard normal maps)
		float nz = sqrtf(std::max(0.0f, 1.0f - nx * nx - ny * ny));

		// Convert normal vector to RGB color [0,255]
		// (0,0,1) normal should map to (128,128,255) = light blue
		outRGBA[i * 4 + 0] = (uint8_t)((nx + 1.0f) * 127.5f);  // R
		outRGBA[i * 4 + 1] = (uint8_t)((ny + 1.0f) * 127.5f);  // G
		outRGBA[i * 4 + 2] = (uint8_t)((nz + 1.0f) * 127.5f);  // B
		outRGBA[i * 4 + 3] = 255;
	}

	// Generate OpenGL texture for preview
	glGenTextures(1, &previewID);
	glBindTexture(GL_TEXTURE_2D, previewID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, outRGBA.data());

	// Set filtering and wrapping
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
}