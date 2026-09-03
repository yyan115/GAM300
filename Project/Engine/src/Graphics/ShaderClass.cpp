#include "pch.h"

#include <filesystem>
#include <cstring>
#include "Graphics/ShaderClass.h"
#include "Logging.hpp"

#ifdef ANDROID
#include <android/asset_manager.h>
#include <android/log.h>
#include "WindowManager.hpp"
#include "Platform/AndroidPlatform.h"
#endif
#include <Utilities/FileUtilities.hpp>
#include <Asset Manager/AssetManager.hpp>
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"

//std::string get_file_contents(const char* filename)
//{
//#ifdef ANDROID
//	// On Android, load from assets
//	__android_log_print(ANDROID_LOG_INFO, "GAM300", "Loading asset: %s", filename);
//
//	auto* platform = WindowManager::GetPlatform();
//	if (!platform) {
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Platform not available for asset loading");
//		throw std::runtime_error("Platform not available");
//	}
//
//	AndroidPlatform* androidPlatform = static_cast<AndroidPlatform*>(platform);
//	AAssetManager* assetManager = androidPlatform->GetAssetManager();
//
//	if (assetManager) {
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "AssetManager is valid, attempting to open: %s", filename);
//		// Try to load from Android assets
//		AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);
//		if (asset) {
//			__android_log_print(ANDROID_LOG_INFO, "GAM300", "Asset opened successfully: %s", filename);
//			size_t length = AAsset_getLength(asset);
//			const char* buffer = (const char*)AAsset_getBuffer(asset);
//			if (buffer) {
//				std::string contents(buffer, length);
//				AAsset_close(asset);
//				__android_log_print(ANDROID_LOG_INFO, "GAM300", "Successfully loaded asset: %s (%zu bytes)", filename, length);
//				return contents;
//			}
//			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to get buffer for asset: %s", filename);
//			AAsset_close(asset);
//		} else {
//			__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to open asset: %s", filename);
//		}
//		__android_log_print(ANDROID_LOG_WARN, "GAM300", "Failed to load asset: %s, trying regular file", filename);
//	} else {
//		__android_log_print(ANDROID_LOG_ERROR, "GAM300", "AssetManager is null!");
//	}
//#endif
//
//	std::ifstream in(filename, std::ios::binary);
//	if (in)
//	{
//		std::string contents;
//		in.seekg(0, std::ios::end);
//		contents.resize(in.tellg());
//		in.seekg(0, std::ios::beg);
//		in.read(&contents[0], contents.size());
//		in.close();
//#ifdef ANDROID
//		__android_log_print(ANDROID_LOG_INFO, "GAM300", "Successfully loaded file: %s", filename);
//#endif
//		return(contents);
//	}
//#ifdef ANDROID
//	__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to load file: %s", filename);
//#endif
//	std::string error_msg = "Failed to open file: " + std::string(filename);
//	if (errno != 0) {
//#ifdef _WIN32
//		char error_buffer[256];
//		strerror_s(error_buffer, sizeof(error_buffer), errno);
//		error_msg += " (Error: " + std::string(error_buffer) + ")";
//#else
//		error_msg += " (Error: " + std::string(strerror(errno)) + ")";
//#endif
//	}
//	throw std::runtime_error(error_msg);
//}

std::string get_file_contents(const char* filename)
{
	auto* platform = WindowManager::GetPlatform();
	if (!platform) {
		throw std::runtime_error("Platform not available");
	}

	ENGINE_LOG_INFO("get_file_contents filename: " + std::string{ filename });
	std::vector<uint8_t> buffer = platform->ReadAsset(filename);
	if (!buffer.empty()) {
		std::string contents(buffer.size(), '\0');
		std::memcpy(&contents[0], buffer.data(), buffer.size());
		return contents;
	}

	return "";
}

bool Shader::SetupShader(const std::string& path) {
#ifdef __ANDROID__
	// Ensure OpenGL context is current for Android
	auto platform = WindowManager::GetPlatform();
	if (platform) {
		if (!platform->MakeContextCurrent()) {
			//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to make OpenGL context current for shader compilation");
			return false;
		}
	}
#endif

	ENGINE_LOG_INFO("SetupShader path: " + path);

	std::filesystem::path p(path);
	std::string genericPath = (p.parent_path() / p.stem()).generic_string();
#ifdef __ANDROID__
	// The opaque mobile PBR permutation shares the maintained default sources.
	// Its virtual asset path only injects a compile-time define, avoiding a
	// second 450-line shader copy that could drift out of sync.
	const bool opaqueDiffuseVariant = p.stem() == "defaultopaqueandroid";
	if (opaqueDiffuseVariant) {
		genericPath =
			(p.parent_path() / "defaultandroid").generic_string();
	}
#endif
	std::string vertexFile = genericPath + ".vert";
	std::string fragmentFile = genericPath + ".frag";
	std::string geometryFile = genericPath + ".geom";  // NEW: Geometry shader path

#ifdef __ANDROID__
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "Loading shader files: %s and %s", vertexFile.c_str(), fragmentFile.c_str());
#endif

	// Read vertexFile and fragmentFile and store the strings
	std::string vertexCode = get_file_contents(vertexFile.c_str());
	std::string fragmentCode = get_file_contents(fragmentFile.c_str());

#ifdef __ANDROID__
	if (opaqueDiffuseVariant) {
		// GLSL requires #version to remain the first directive.
		const std::size_t versionEnd = fragmentCode.find('\n');
		if (versionEnd == std::string::npos) {
			return false;
		}
		fragmentCode.insert(
			versionEnd + 1,
			"#define GAM300_OPAQUE_DIFFUSE 1\n");
	}
#endif

#ifndef __ANDROID__
	// NEW: Try to read geometry shader (optional)
	std::string geometryCode;
	bool hasGeometryShader = false;

	// Check if geometry shader file exists before trying to load
	if (std::filesystem::exists(geometryFile)) {
		geometryCode = get_file_contents(geometryFile.c_str());
		hasGeometryShader = !geometryCode.empty();
		if (hasGeometryShader) {
			ENGINE_LOG_INFO("Found geometry shader: " + geometryFile);
		}
		else {
			ENGINE_PRINT("[SHADER] Warning: Geometry shader file exists but is empty: ", geometryFile, "\n");
		}
	}
	else {
		// Geometry shader is optional - not an error if it doesn't exist
		ENGINE_LOG_INFO("No geometry shader found at: " + geometryFile + " (optional)");
	}
#endif

	// Convert the shader source strings into character arrays
	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	// ========================================================================
	// VERTEX SHADER
	// ========================================================================
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
#ifdef __ANDROID__
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "Compiling vertex shader with %d chars", (int)strlen(vertexSource));
#endif
	glCompileShader(vertexShader);

	GLint success = GL_FALSE;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

	if (success != GL_TRUE)
	{
		GLint logLength = 0;
		glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);

		if (logLength > 1) {
			std::vector<char> errorLog(logLength + 1);
			GLsizei actualLength = 0;
			glGetShaderInfoLog(vertexShader, logLength, &actualLength, &errorLog[0]);
			errorLog[actualLength] = '\0';
			ENGINE_PRINT("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n", &errorLog[0], "\n");
		}
		else {
			ENGINE_PRINT("ERROR::SHADER::VERTEX::COMPILATION_FAILED - No error log available", "\n");
		}
		glDeleteShader(vertexShader);
		return false;
	}

	// ========================================================================
	// GEOMETRY SHADER (NEW - OPTIONAL)
	// ========================================================================
#ifndef __ANDROID__
	GLuint geometryShader = 0;
	if (hasGeometryShader)
	{
		const char* geometrySource = geometryCode.c_str();
		geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometryShader, 1, &geometrySource, NULL);
#ifdef __ANDROID__
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "Compiling geometry shader with %d chars", (int)strlen(geometrySource));
#endif
		glCompileShader(geometryShader);

		GLint geomSuccess = GL_FALSE;
		glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &geomSuccess);

		if (geomSuccess != GL_TRUE)
		{
			GLint logLength = 0;
			glGetShaderiv(geometryShader, GL_INFO_LOG_LENGTH, &logLength);

			if (logLength > 1) {
				std::vector<char> errorLog(logLength + 1);
				GLsizei actualLength = 0;
				glGetShaderInfoLog(geometryShader, logLength, &actualLength, &errorLog[0]);
				errorLog[actualLength] = '\0';
				ENGINE_PRINT("ERROR::SHADER::GEOMETRY::COMPILATION_FAILED\n", &errorLog[0], "\n");
#ifdef __ANDROID__
				//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Geometry shader compilation failed: %s", &errorLog[0]);
#endif
			}
			else {
				ENGINE_PRINT("ERROR::SHADER::GEOMETRY::COMPILATION_FAILED - No error log available", "\n");
			}
			glDeleteShader(vertexShader);
			glDeleteShader(geometryShader);
			return false;
		}

		ENGINE_PRINT("[SHADER] Geometry shader compiled successfully\n");
	}
#endif

	// ========================================================================
	// FRAGMENT SHADER
	// ========================================================================
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
#ifdef __ANDROID__
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "Compiling fragment shader with %d chars", (int)strlen(fragmentSource));
#endif
	glCompileShader(fragmentShader);

	GLint fragmentSuccess = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentSuccess);

	if (fragmentSuccess != GL_TRUE)
	{
		GLint logLength = 0;
		glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLength);

		if (logLength > 1) {
			std::vector<char> errorLog(logLength + 1);
			GLsizei actualLength = 0;
			glGetShaderInfoLog(fragmentShader, logLength, &actualLength, &errorLog[0]);
			errorLog[actualLength] = '\0';
			ENGINE_PRINT("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n", &errorLog[0], "\n");
		}
		else {
			ENGINE_PRINT("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED - No error log available", "\n");
		}
		glDeleteShader(vertexShader);
#ifndef __ANDROID__
		if (hasGeometryShader) glDeleteShader(geometryShader);
#endif
		glDeleteShader(fragmentShader);
		return false;
	}

	// ========================================================================
	// SHADER PROGRAM LINKING
	// ========================================================================
	ID = glCreateProgram();
#ifndef __ANDROID__
	glProgramParameteri(ID, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
#endif

	// Attach all shaders
	glAttachShader(ID, vertexShader);
#ifndef __ANDROID__
	if (hasGeometryShader) {
		glAttachShader(ID, geometryShader);
	}
#endif
	glAttachShader(ID, fragmentShader);

	// Link the program
	glLinkProgram(ID);

	// Check for linking errors
	GLint linkSuccess = GL_FALSE;
	glGetProgramiv(ID, GL_LINK_STATUS, &linkSuccess);
#ifdef __ANDROID__
	//__android_log_print(ANDROID_LOG_INFO, "GAM300", "Link status: %d (GL_TRUE=%d)", linkSuccess, GL_TRUE);
#endif

	if (linkSuccess != GL_TRUE) {
		GLint logLength = 0;
		glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 1) {
			std::vector<char> infoLog(logLength + 1);
			GLsizei actualLength = 0;
			glGetProgramInfoLog(ID, logLength, &actualLength, &infoLog[0]);
			infoLog[actualLength] = '\0';
			ENGINE_PRINT("ERROR::SHADER::PROGRAM::LINKING_FAILED\n", &infoLog[0], "\n");
#ifdef __ANDROID__
			//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "Shader program linking failed: %s", &infoLog[0]);
#endif
		}
		glDeleteShader(vertexShader);
#ifndef __ANDROID__
		if (hasGeometryShader) glDeleteShader(geometryShader);
#endif
		glDeleteShader(fragmentShader);
		glDeleteProgram(ID);
		ID = 0;
		return false;
	}

	// Clean up shader objects
	glDeleteShader(vertexShader);
#ifndef __ANDROID__
	if (hasGeometryShader) {
		glDeleteShader(geometryShader);
	}
#endif
	glDeleteShader(fragmentShader);

    BindKnownUniformBlocks();

	return true;
}

Shader::Shader(std::shared_ptr<AssetMeta> shaderMeta) {
	shaderMeta;
}

std::string Shader::CompileToResource(const std::string& path, bool forAndroid) {
	//// Check if glGetProgramBinary is supported first.
	//GLint supported = 0;
	//glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &supported);
	//if (supported == 0) {
	//	std::cerr << "[SHADER]: Program binary not supported. Skipping binary cache.\n";
	//	binarySupported = false;
	//	return std::string{};
	//}

	binarySupported = true;
	std::filesystem::path p(path);
	// Save the source base path before p gets reassigned later.
	// This is needed to copy .vert/.frag source files for Android fallback.
	std::string sourceBasePath = (p.parent_path() / p.stem()).generic_string();

	if (!SetupShader(path)) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER]: Shader compilation failed. Aborting resource compilation.\n");
		return std::string{};
	}

	// Retrieve the binary code of the compiled shader.
	glGetProgramiv(ID, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

	binaryData.resize(binaryLength);
	glGetProgramBinary(ID, binaryLength, nullptr, &binaryFormat, binaryData.data());

	// Save the binary code to a file.
	std::string shaderPath{}; 
	
	if (!forAndroid)
		shaderPath = (p.parent_path() / p.stem()).generic_string() + ".shader";
	else {
		std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
		assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
		shaderPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + ".shader";
	}

	// Ensure parent directories exist
	p = shaderPath;
	std::filesystem::create_directories(p.parent_path());
	std::ofstream shaderFile(shaderPath, std::ios::binary);
#ifndef ANDROID
	// If the install directory is read-only (e.g. Program Files), fall back to LocalAppData
	if (!shaderFile.is_open()) {
		const char* localAppData = getenv("LOCALAPPDATA");
		if (localAppData) {
			p = std::filesystem::path(localAppData) / "DigiPen" / "Kusane" / "ShaderCache" / std::filesystem::path(shaderPath).filename();
			std::filesystem::create_directories(p.parent_path());
			shaderFile.open(p, std::ios::binary);
			if (shaderFile.is_open())
				shaderPath = p.generic_string();
		}
	}
#endif
	if (shaderFile.is_open()) {
		// Write the binary format to the file.
		shaderFile.write(reinterpret_cast<const char*>(&binaryFormat), sizeof(binaryFormat));
		// Write the binary length to the file.
		shaderFile.write(reinterpret_cast<const char*>(&binaryLength), sizeof(binaryLength));
		// Write the binary code to the file.
		shaderFile.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
		shaderFile.close();

		if (forAndroid) {
			// Copy the .vert and .frag source files as well for fallback.
			try {
				std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
				assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));

				std::string vertPath = assetPathAndroid + ".vert";
				std::string androidVertPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / vertPath).generic_string();
				std::filesystem::path newVertPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(androidVertPath));
				androidVertPath = newVertPath.generic_string();
				std::string fragPath = assetPathAndroid + ".frag";
				std::string androidFragPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / fragPath).generic_string();
				std::filesystem::path newFragPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(androidFragPath));
				androidFragPath = newFragPath.generic_string();
				// sourceBasePath was saved before p was reassigned to the android shader path
				std::filesystem::copy_file(sourceBasePath + ".vert", androidVertPath,
					std::filesystem::copy_options::overwrite_existing);
				std::filesystem::copy_file(sourceBasePath + ".frag", androidFragPath,
					std::filesystem::copy_options::overwrite_existing);
			}
			catch (const std::filesystem::filesystem_error& e) {
				ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Asset] Copy failed: ", e.what(), "\n");
				return std::string{};
			}

		}
	}

	return shaderPath;

	//if (!forAndroid) {
	//	// Save the binary code to the root project folder as well.
	//	p = (FileUtilities::GetSolutionRootDir() / shaderPath);
	//	shaderFile.open(p.generic_string(), std::ios::binary);
	//	if (shaderFile.is_open()) {
	//		// Write the binary format to the file.
	//		shaderFile.write(reinterpret_cast<const char*>(&binaryFormat), sizeof(binaryFormat));
	//		// Write the binary length to the file.
	//		shaderFile.write(reinterpret_cast<const char*>(&binaryLength), sizeof(binaryLength));
	//		// Write the binary code to the file.
	//		shaderFile.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
	//		shaderFile.close();
	//		return shaderPath;
	//	}
	//}

	//return std::string{};
}

bool Shader::LoadResource(const std::string& resourcePath, const std::string& assetPath)
{
#ifdef ANDROID
	// OpenGL ES program binaries are vendor/driver specific and the current
	// asset format carries no source hash. Loading a packaged binary can
	// therefore silently select stale code on a compatible device. Compile the
	// packaged GLSL source once at startup; Android drivers retain their own
	// implementation-specific shader cache.
	const std::string& sourcePath =
		assetPath.empty() ? resourcePath : assetPath;
	return SetupShader(sourcePath);
#else
	(void)assetPath;
#endif

	if (!binarySupported) {
		// Fallback to regular shader compilation if binary is not supported.
		if (!SetupShader(resourcePath)) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER]: Shader compilation failed. Aborting load.\n");
			return false;
		}

		return true;
	}

	// Use platform abstraction to get asset list (works on Windows, Linux, Android)
	IPlatform* platform = WindowManager::GetPlatform();
	if (!platform) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER] ERROR: Platform not available for asset discovery!", "\n");
		return false;
	}

	std::vector<uint8_t> fileData = platform->ReadAsset(resourcePath);
	if (!fileData.empty()) {
		size_t offset = 0;

		// Read binaryFormat (GLuint)
		if (offset + sizeof(binaryFormat) > fileData.size()) return false;
		std::memcpy(&binaryFormat, fileData.data() + offset, sizeof(binaryFormat));
		offset += sizeof(binaryFormat);

		// Read binaryLength (GLsizei)
		if (offset + sizeof(binaryLength) > fileData.size()) return false;
		std::memcpy(&binaryLength, fileData.data() + offset, sizeof(binaryLength));
		offset += sizeof(binaryLength);

		// Read binaryData
		if (offset + binaryLength > fileData.size()) return false;
		binaryData.resize(binaryLength);
		std::memcpy(binaryData.data(), fileData.data() + offset, binaryLength);
		offset += binaryLength;

		// Create a new shader program.
		ID = glCreateProgram();
		glProgramBinary(ID, binaryFormat, binaryData.data(), binaryLength);

		// Check if the program was successfully loaded.
		GLint status = 0;
		glGetProgramiv(ID, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER]: Failed to load shader program from binary. Recompiling shader...\n");
#ifndef ANDROID
			std::string newCachePath = CompileToResource(assetPath);
			if (newCachePath.empty()) {
				ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER]: Recompilation failed. Aborting load.\n");
				return false;
			}
			return LoadResource(newCachePath, assetPath);
#else
			if (!SetupShader(assetPath)) {
				ENGINE_LOG_INFO("[SHADER]: Android shader setup failed. Aborting load.");
				return false;
			}
			else return true;
#endif
		}

        BindKnownUniformBlocks();

		return true;
	}
	//std::ifstream shaderFile(resourcePath, std::ios::binary);
	//if (shaderFile.is_open()) {
	//	// Read the binary format from the file.
	//	shaderFile.read(reinterpret_cast<char*>(&binaryFormat), sizeof(binaryFormat));
	//	// Read the binary length from the file.
	//	shaderFile.read(reinterpret_cast<char*>(&binaryLength), sizeof(binaryLength));
	//	binaryData.resize(binaryLength);
	//	// Read the binary code from the file.
	//	shaderFile.read(reinterpret_cast<char*>(binaryData.data()), binaryLength);

	//	// Create a new shader program.
	//	ID = glCreateProgram();
	//	glProgramBinary(ID, binaryFormat, binaryData.data(), binaryLength);

	//	// Check if the program was successfully loaded.
	//	GLint status = 0;
	//	glGetProgramiv(ID, GL_LINK_STATUS, &status);
	//	if (status == GL_FALSE) {
	//		std::cerr << "[SHADER]: Failed to load shader program from binary. Recompiling shader..." << std::endl;
	//		if (CompileToResource(resourcePath).empty()) {
	//			std::cerr << "[SHADER]: Recompilation failed. Aborting load." << std::endl;
	//			return false;
	//		}

	//		return LoadResource(resourcePath);
	//	}

	//	return true;
	//}
	else {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER]: Shader file not found: ", resourcePath, ", attempting to compile from source", "\n");
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_WARN, "GAM300", "[SHADER]: Shader file not found: %s, attempting to compile from source", resourcePath.c_str());
#endif
		// Fallback to regular shader compilation if binary file is not found
		if (!SetupShader(resourcePath)) {
			ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SHADER]: Shader compilation from source failed. Aborting load.", "\n");
#ifdef ANDROID
			//__android_log_print(ANDROID_LOG_ERROR, "GAM300", "[SHADER]: Shader compilation from source failed. Aborting load.");
#endif
			return false;
		}
#ifdef ANDROID
		//__android_log_print(ANDROID_LOG_INFO, "GAM300", "[SHADER]: Successfully compiled shader from source: %s", assetPath.c_str());
#endif
		ENGINE_PRINT("[SHADER]: Successfully compiled shader from source: ", resourcePath, "\n");
		return true;
	}
}

bool Shader::ReloadResource(const std::string& resourcePath, const std::string& assetPath)
{
	return LoadResource(resourcePath, assetPath);
}

std::shared_ptr<AssetMeta> Shader::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid)
{
	assetPath, currentMetaData, forAndroid;
	return currentMetaData;
}

void Shader::Activate()
{
	if (s_activeProgram != ID)
	{
		glUseProgram(ID);
		s_activeProgram = ID;
	}
}

void Shader::BindKnownUniformBlocks()
{
    // A reload creates a new program, so cached locations and block metadata
    // from the previous program are no longer valid.
    m_uniformCache.clear();
    m_uniformValueCache.clear();
    m_hasStaticShadowUniforms = false;

    const GLuint cameraBlock = glGetUniformBlockIndex(ID, "CameraBlock");
    m_usesCameraBlock = cameraBlock != GL_INVALID_INDEX;
    if (m_usesCameraBlock) {
        glUniformBlockBinding(ID, cameraBlock, 0);
    }

    const GLuint lightingBlock = glGetUniformBlockIndex(ID, "LightingBlock");
    m_usesLightingBlock = lightingBlock != GL_INVALID_INDEX;
    if (m_usesLightingBlock) {
        glUniformBlockBinding(ID, lightingBlock, 1);
    }

    const GLuint bonesBlock = glGetUniformBlockIndex(ID, "BonesBlock");
    m_usesBonesBlock = bonesBlock != GL_INVALID_INDEX;
    if (m_usesBonesBlock) {
        glUniformBlockBinding(ID, bonesBlock, 2);
    }

    const GLuint materialBlock = glGetUniformBlockIndex(ID, "MaterialBlock");
    m_usesMaterialBlock = materialBlock != GL_INVALID_INDEX;
    if (m_usesMaterialBlock) {
        glUniformBlockBinding(ID, materialBlock, 3);
    }
}

void Shader::Delete()
{
	if (ID == 0) {
		return;
	}
	if (s_activeProgram == ID) {
		s_activeProgram = 0;
	}
	glDeleteProgram(ID);
	ID = 0;
	m_uniformCache.clear();
	m_uniformValueCache.clear();
	m_usesCameraBlock = false;
	m_usesLightingBlock = false;
	m_usesBonesBlock = false;
	m_usesMaterialBlock = false;
	m_hasStaticShadowUniforms = false;
}

void Shader::setBool(std::string_view name, GLboolean value)
{
	GLint location = getUniformLocation(name);
	const int intValue = static_cast<int>(value);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Int, &intValue, sizeof(intValue))) {
		glUniform1i(location, intValue);
	}
}

void Shader::setInt(std::string_view name, int value)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Int, &value, sizeof(value))) {
		glUniform1i(location, value);
	}
}

void Shader::setIntArray(std::string_view name, const GLint* values, GLint count)
{
	GLint location = getUniformLocation(name);
	const std::size_t byteCount =
		count > 0 ? static_cast<std::size_t>(count) * sizeof(GLint) : 0;
	if (location != -1 && values && count > 0 &&
		(byteCount > sizeof(CachedUniformValue::bytes) ||
		 ShouldUploadUniform(
			 location, UniformValueType::IntArray, values, byteCount))) {
		glUniform1iv(location, count, values);
	}
}

void Shader::setFloat(std::string_view name, GLfloat value)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Float, &value, sizeof(value))) {
		glUniform1f(location, value);
	}
}

void Shader::setVec2(std::string_view name, const glm::vec2& value)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Vec2, &value[0], sizeof(float) * 2)) {
		glUniform2fv(location, 1, &value[0]);
	}
}

void Shader::setVec2(std::string_view name, float x, float y)
{
	GLint location = getUniformLocation(name);
	const float value[2] = { x, y };
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Vec2, value, sizeof(value))) {
		glUniform2f(location, x, y);
	}
}

void Shader::setVec3(std::string_view name, const glm::vec3& value)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Vec3, &value[0], sizeof(float) * 3)) {
		glUniform3fv(location, 1, &value[0]);
	}
}

void Shader::setVec3(std::string_view name, float x, float y, float z)
{
	GLint location = getUniformLocation(name);
	const float value[3] = { x, y, z };
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Vec3, value, sizeof(value))) {
		glUniform3f(location, x, y, z);
	}
}

void Shader::setVec4(std::string_view name, const glm::vec4& value)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Vec4, &value[0], sizeof(float) * 4)) {
		glUniform4fv(location, 1, &value[0]);
	}
}

void Shader::setVec4(std::string_view name, float x, float y, float z, float w)
{
	GLint location = getUniformLocation(name);
	const float value[4] = { x, y, z, w };
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Vec4, value, sizeof(value))) {
		glUniform4f(location, x, y, z, w);
	}
}

void Shader::setMat2(std::string_view name, const glm::mat2& mat)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Mat2, &mat[0][0], sizeof(float) * 4)) {
		glUniformMatrix2fv(location, 1, GL_FALSE, &mat[0][0]);
	}
}

void Shader::setMat3(std::string_view name, const glm::mat3& mat)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Mat3, &mat[0][0], sizeof(float) * 9)) {
		glUniformMatrix3fv(location, 1, GL_FALSE, &mat[0][0]);
	}
}

void Shader::setMat4(std::string_view name, const glm::mat4& mat)
{
	GLint location = getUniformLocation(name);
	if (location != -1 &&
		ShouldUploadUniform(location, UniformValueType::Mat4, &mat[0][0], sizeof(float) * 16)) {
		glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
	}
}

void Shader::setMat4Array(std::string_view firstName, const glm::mat4* matrices, GLsizei count)
{
	if (count <= 0) return;
	// Look up the base element once — GL treats array uniforms as contiguous
	// locations, so uploading from [0] with count>1 fills the entire array.
	GLint location = getUniformLocation(firstName);
	if (location != -1) {
		glUniformMatrix4fv(location, count, GL_FALSE, glm::value_ptr(matrices[0]));
	}
}

GLint Shader::getUniformLocation(std::string_view name)
{
	auto it = m_uniformCache.find(name);
	if (it != m_uniformCache.end())
	{
		return it->second;
	}

	auto cacheIt = m_uniformCache.emplace(std::string(name), -1).first;
	GLint location = glGetUniformLocation(ID, cacheIt->first.c_str());
	cacheIt->second = location;

	// Debug output for missing uniforms (can be removed later)
	if (location == -1)
	{
		ENGINE_PRINT("Warning: Uniform '" , name , "' not found in shader ID: ", ID, "\n");
	}

	return location;
}

bool Shader::ShouldUploadUniform(
	GLint location,
	UniformValueType type,
	const void* data,
	std::size_t size)
{
	if (location < 0 || !data || size == 0 ||
		size > sizeof(CachedUniformValue::bytes)) {
		return true;
	}

	const std::size_t index = static_cast<std::size_t>(location);
	if (index >= m_uniformValueCache.size()) {
		m_uniformValueCache.resize(index + 1);
	}

	CachedUniformValue& cached = m_uniformValueCache[index];
	if (cached.valid &&
		cached.type == type &&
		cached.size == size &&
		std::memcmp(cached.bytes.data(), data, size) == 0) {
		return false;
	}

	std::memcpy(cached.bytes.data(), data, size);
	cached.type = type;
	cached.size = static_cast<std::uint8_t>(size);
	cached.valid = true;
	return true;
}
