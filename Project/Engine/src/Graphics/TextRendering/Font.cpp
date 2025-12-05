#include "pch.h"
#include "Graphics/TextRendering/Font.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include <Asset Manager/AssetManager.hpp>
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>
#include "Logging.hpp"

Font::Font(unsigned int fontSize) : fontSize(fontSize) {}

Font::Font(std::shared_ptr<AssetMeta> fontMeta, unsigned int fontSize) : fontSize(fontSize) {
    fontMeta;
}

Font::~Font()
{
	Cleanup();
}

std::string Font::CompileToResource(const std::string& assetPath, bool forAndroid)
{
    if (!std::filesystem::exists(assetPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] File does not exist: " , assetPath, "\n");
        return std::string{};
    }

    if (std::filesystem::is_directory(assetPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Path is a directory, not a file: ", assetPath, "\n");
        return std::string{};
    }

    // Load font data from file and extract raw binary data.
    std::ifstream fontAsset(assetPath, std::ios::binary | std::ios::ate); // Use std::ios::ate to query data size
    if (!fontAsset.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to open font asset: ", assetPath, "\n");
        return std::string{};
    }

    std::streamsize size = fontAsset.tellg();
    fontAsset.seekg(0, std::ios::beg); // Return to beginning of file

    std::vector<unsigned char> fontData(size);
    fontAsset.read(reinterpret_cast<char*>(fontData.data()), size);

    fontAsset.close();

    // Write raw binary data to an output file.
    std::filesystem::path p(assetPath);
    std::string outPath{};
    
    if (!forAndroid) {
        outPath = (p.parent_path() / p.stem()).generic_string() + ".font";
    }
    else {
        std::string assetPathAndroid = (p.parent_path() / p.stem()).generic_string();
        assetPathAndroid = assetPathAndroid.substr(assetPathAndroid.find("Resources"));
        outPath = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string() + "_android.font";
        std::filesystem::path newPath = FileUtilities::SanitizePathForAndroid(std::filesystem::path(outPath));
        outPath = newPath.generic_string();
    }

    // Ensure parent directories exist
    p = outPath;
    std::filesystem::create_directories(p.parent_path());
    std::ofstream fontResource(outPath, std::ios::binary);
    if (!fontResource.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to write output font resource: ", outPath, "\n");
        return std::string{};
    }

    fontResource.write(reinterpret_cast<const char*>(fontData.data()), size);
    fontResource.close();

    //if (!forAndroid) {
    //    // Save the mesh file to the root project Resources folder as well.
    //    try {
    //        std::filesystem::copy_file(outPath, (FileUtilities::GetSolutionRootDir() / outPath).generic_string(),
    //            std::filesystem::copy_options::overwrite_existing);
    //    }
    //    catch (const std::filesystem::filesystem_error& e) {
    //        std::cerr << "[FONT] Copy failed: " << e.what() << std::endl;
    //    }
    //}

    return outPath;
}

bool Font::LoadResource(const std::string& resourcePath,
    const std::string& assetPath,
    unsigned int newFontSize,
    bool setFontSize)
{
    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] LoadResource called with resourcePath=", resourcePath,
        " assetPath=", assetPath,
        " newFontSize=", newFontSize,
        " setFontSize=", setFontSize, "\n");

    if (setFontSize)
        fontSize = newFontSize;
    fontAssetPath = assetPath;
    fontResourcePath = resourcePath;

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Cleanup existing font data\n");
    Cleanup();

    // Initialize FreeType
    FT_Library ft;
    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Initializing FreeType library...\n");
    if (FT_Init_FreeType(&ft))
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error,
            "[Font] Could not initialize FreeType Library\n");
        return false;
    }
    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] FreeType initialized successfully\n");

    // Load font using platform abstraction
    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error,
            "[Font] ERROR: Platform not available for asset discovery!\n");
        return false;
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Reading asset via platform: ", resourcePath, "\n");
    buffer = platform->ReadAsset(resourcePath);
    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Buffer size after ReadAsset=", buffer.size(), "\n");

    FT_Face face{};
    if (!buffer.empty()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Debug,
            "[Font] Creating FreeType face from memory buffer...\n");
        FT_Error error = FT_New_Memory_Face(
            ft,
            reinterpret_cast<const FT_Byte*>(buffer.data()),
            static_cast<FT_Long>(buffer.size()),
            0,
            &face
        );

        if (error) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error,
                "[Font] Failed to load font resource: ", resourcePath,
                " FreeType error code=", error, "\n");
            FT_Done_FreeType(ft);
            return false;
        }
        else {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug,
                "[Font] FreeType face created successfully\n");
        }
    }
    else {
        ENGINE_PRINT(EngineLogging::LogLevel::Error,
            "[Font] Buffer empty, cannot create FreeType face\n");
        FT_Done_FreeType(ft);
        return false;
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Setting pixel sizes: height=", fontSize, "\n");
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Loading glyphs...\n");
    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            ENGINE_PRINT(EngineLogging::LogLevel::Error,
                "[Font] Failed to load Glyph for character code=", (int)c, "\n");
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));

        ENGINE_PRINT(EngineLogging::LogLevel::Trace,
            "[Font] Loaded glyph for char=", (int)c,
            " texID=", texture,
            " size=", face->glyph->bitmap.width, "x", face->glyph->bitmap.rows, "\n");
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Cleaning up FreeType face and library\n");
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    ENGINE_PRINT(EngineLogging::LogLevel::Debug,
        "[Font] Setting up VAO/VBO for text rendering\n");
    textVAO = std::make_unique<VAO>();
    textVBO = std::make_unique<VBO>(sizeof(float) * 6 * 4, GL_DYNAMIC_DRAW);

    textVAO->Bind();
    textVBO->Bind();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    textVBO->Unbind();
    textVAO->Unbind();

    ENGINE_PRINT(EngineLogging::LogLevel::Info,
        "[Font] Successfully loaded font resource: ", resourcePath,
        " (size: ", fontSize, ")\n");
    return true;
}


bool Font::ReloadResource(const std::string& resourcePath, const std::string& assetPath)
{
    // When reloading fonts, don't reset the font size (keep the current font size).
    return LoadResource(resourcePath, assetPath, 0, false);
}

//bool Font::LoadFont(const std::string& path, unsigned int fontSizeParam)
//{
//    // Store font info
//    fontPath = path;
//    fontSize = fontSizeParam;
//
//    // Clean up existing font data if any
//    Cleanup();
//
//    // Initialize FreeType
//    FT_Library ft;
//    if (FT_Init_FreeType(&ft)) 
//    {
//        std::cerr << "[Font] Could not initialize FreeType Library" << std::endl;
//        return false;
//    }
// 
//    // Load font as face
//    FT_Face face;
//    if (FT_New_Face(ft, path.c_str(), 0, &face)) 
//    {
//        std::cerr << "[Font] Failed to load font: " << path << std::endl;
//        FT_Done_FreeType(ft);
//        return false;
//    }
//
//    // Sets the font's width and height parameters
//    // Setting the width to 0 lets the face dynamically calculate the width based on the given height
//    FT_Set_Pixel_Sizes(face, 0, fontSize);
//
//    // Disable byte-alignment restriction
//    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//
//    // Load first 128 characters of ASCII set
//    for (unsigned char c = 0; c < 128; c++) 
//    {
//        // Load character glyph
//        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) 
//        {
//            std::cerr << "[Font] Failed to load Glyph for character: " << c << std::endl;
//            continue;
//        }
//
//        // Generate texture
//        unsigned int texture;
//        glGenTextures(1, &texture);
//        glBindTexture(GL_TEXTURE_2D, texture);
//        glTexImage2D(
//            GL_TEXTURE_2D,
//            0,
//            GL_RED,
//            face->glyph->bitmap.width,
//            face->glyph->bitmap.rows,
//            0,
//            GL_RED,
//            GL_UNSIGNED_BYTE,
//            face->glyph->bitmap.buffer
//        );
//
//        // Set texture options
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//        // Store character for later use
//        Character character = {
//            texture,
//            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
//            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
//            static_cast<unsigned int>(face->glyph->advance.x)
//        };
//        Characters.insert(std::pair<char, Character>(c, character));
//    }
//    glBindTexture(GL_TEXTURE_2D, 0);
//
//    // Destroy FreeType once we're finished
//    FT_Done_Face(face);
//    FT_Done_FreeType(ft);
//
//    // Set up VAO/VBO for text rendering using your extended classes
//    textVAO = std::make_unique<VAO>();
//    textVBO = std::make_unique<VBO>(sizeof(float) * 6 * 4, GL_DYNAMIC_DRAW);
//
//    textVAO->Bind();
//    textVBO->Bind();
//
//    // Set up vertex attributes for text (vec4: x, y, u, v)
//    glEnableVertexAttribArray(0);
//    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
//
//    textVBO->Unbind();
//    textVAO->Unbind();
//
//    std::cout << "[Font] Successfully loaded font: " << path << " (size: " << fontSize << ")" << std::endl;
//    return true;
//}

void Font::SetFontSize(unsigned int newSize)
{
    if (newSize != fontSize && !fontAssetPath.empty()) 
    {
        LoadResource(fontResourcePath, fontAssetPath, newSize);
    }
}

const Character& Font::GetCharacter(char c) const
{
    auto it = Characters.find(c);
    if (it != Characters.end()) 
    {
        return it->second;
    }

    // Return space character as fallback
    static Character fallback = {};
    return fallback;
}

float Font::GetTextWidth(const std::string& text, float scale) const
{
    float width = 0.0f;
    for (char c : text) 
    {
        const Character& ch = GetCharacter(c);
        width += (ch.advance >> 6) * scale; // Bitshift by 6 to get value in pixels (2^6 = 64)
    }
    return width;
}

float Font::GetTextHeight(float scale) const
{
    float maxHeight = 0.0f;
    for (const auto& pair : Characters) 
    {
        float height = pair.second.size.y * scale;
        if (height > maxHeight) 
        {
            maxHeight = height;
        }
    }
    return maxHeight;
}

void Font::Cleanup()
{
    // Clean up textures
    for (auto& pair : Characters) 
    {
        glDeleteTextures(1, &pair.second.textureID);
    }
    Characters.clear();

    // Clean up VAO/VBO
    if (textVAO) 
    {
        textVAO->Delete();
        textVAO.reset();
    }
    if (textVBO) 
    {
        textVBO->Delete();
        textVBO.reset();
    }
}

std::shared_ptr<AssetMeta> Font::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData, bool forAndroid)
{
    assetPath, currentMetaData, forAndroid;
    return currentMetaData;
}