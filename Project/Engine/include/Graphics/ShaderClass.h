#pragma once

#include "pch.h"

#include "OpenGL.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

std::string get_file_contents(const char* filename);

class ENGINE_API Shader : public IAsset {
public:
    GLuint ID{};

    //Shader() {};
	//Shader(const char* vertexFile, const char* fragmentFile);

    ENGINE_API std::string CompileToResource(const std::string& path) override;
    ENGINE_API bool LoadResource(const std::string& assetPath) override;
    ENGINE_API std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData) override;

	void Activate();
	void Delete();

    void setBool(const std::string& name, GLboolean value);
    void setInt(const std::string& name, int value);
    void setIntArray(const std::string& name, const GLint* values, GLint count);
    void setFloat(const std::string& name, GLfloat value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat2(const std::string& name, const glm::mat2& mat);
    void setMat3(const std::string& name, const glm::mat3& mat);
    void setMat4(const std::string& name, const glm::mat4& mat);

    //void clearUniformCache();

private:
    std::unordered_map<std::string, GLint> m_uniformCache;
    GLint getUniformLocation(const std::string& name);

    // Store shader binary data (precompiled shader).
    GLint binaryLength{};
	std::vector<uint8_t> binaryData;
    GLenum binaryFormat{};
    bool binarySupported = true;

    bool SetupShader(const std::string& path);
};
