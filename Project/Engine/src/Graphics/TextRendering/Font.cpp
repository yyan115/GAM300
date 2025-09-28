#include "pch.h"
#include "Graphics/TextRendering/Font.hpp"
#include "Graphics/VAO.h"
#include "Graphics/VBO.h"
#include "Logging.hpp"

Font::Font(unsigned int fontSize) : fontSize(fontSize) {}

Font::~Font()
{
	Cleanup();
}

std::string Font::CompileToResource(const std::string& assetPath)
{
    if (!std::filesystem::exists(assetPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] File does not exist: " , assetPath, "\n");
        //std::cerr << "[Font] File does not exist: " << assetPath << std::endl;
        return std::string{};
    }

    if (std::filesystem::is_directory(assetPath)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Path is a directory, not a file: ", assetPath, "\n");
        //std::cerr << "[Font] Path is a directory, not a file: " << assetPath << std::endl;
        return std::string{};
    }

    // Load font data from file and extract raw binary data.
    std::ifstream fontAsset(assetPath, std::ios::binary | std::ios::ate); // Use std::ios::ate to query data size
    if (!fontAsset.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to open font asset: ", assetPath, "\n");
        //std::cerr << "[Font] Failed to open font asset: " << assetPath << std::endl;
        return std::string{};
    }

    std::streamsize size = fontAsset.tellg();
    fontAsset.seekg(0, std::ios::beg); // Return to beginning of file

    std::vector<unsigned char> fontData(size);
    fontAsset.read(reinterpret_cast<char*>(fontData.data()), size);

    fontAsset.close();

    // Write raw binary data to an output file.
    std::filesystem::path p(assetPath);
    std::string outname = (p.parent_path() / p.stem()).generic_string() + ".font";
    std::ofstream fontResource(outname, std::ios::binary);
    if (!fontResource.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to write output font resource: ", outname, "\n");
        //std::cerr << "[Font] Failed to write output font resource: " << outname << std::endl;
        return std::string{};
    }

    fontResource.write(reinterpret_cast<const char*>(fontData.data()), size);
    fontResource.close();

    return outname;
}

bool Font::LoadResource(const std::string& assetPath, unsigned int newFontSize, bool setFontSize)
{
    if (setFontSize)
        fontSize = newFontSize;
    fontAssetPath = assetPath;

    // Clean up existing font data if any
    Cleanup();

    // Get the .font resource file path.
    std::filesystem::path assetPathFS(assetPath);
    std::string path = (assetPathFS.parent_path() / assetPathFS.stem()).generic_string() + ".font";

    // Read the .font resource file as binary.
    std::ifstream fontFile(path, std::ios::binary | std::ios::ate);
    if (!fontFile.is_open()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to open font asset: ", assetPath, "\n");
        //std::cerr << "[Font] Failed to open font asset: " << assetPath << std::endl;
        return false;
    }

    std::streamsize size = fontFile.tellg();
    fontFile.seekg(0, std::ios::beg); // Return to beginning of file

    // Store font data.
    std::vector<unsigned char> fontData(size);
    fontFile.read(reinterpret_cast<char*>(fontData.data()), size);

    fontFile.close();

    // Initialize FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Could not initialize FreeType Library\n");
        //std::cerr << "[Font] Could not initialize FreeType Library" << std::endl;
        return false;
    }

    // Load font using font data from the binary file.
    FT_Face face;
    if (FT_New_Memory_Face(ft, fontData.data(), static_cast<FT_Long>(fontData.size()), 0, &face))
    {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to load font resource: ", path, "\n");
        //std::cerr << "[Font] Failed to load font resource: " << path << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }

    // Sets the font's width and height parameters
    // Setting the width to 0 lets the face dynamically calculate the width based on the given height
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 characters of ASCII set
    for (unsigned char c = 0; c < 128; c++)
    {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Font] Failed to load Glyph for character: ", c, "\n");
            //std::cerr << "[Font] Failed to load Glyph for character: " << c << std::endl;
            continue;
        }

        // Generate texture
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

        // Set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Set up VAO/VBO for text rendering using your extended classes
    textVAO = std::make_unique<VAO>();
    textVBO = std::make_unique<VBO>(sizeof(float) * 6 * 4, GL_DYNAMIC_DRAW);

    textVAO->Bind();
    textVBO->Bind();

    // Set up vertex attributes for text (vec4: x, y, u, v)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    textVBO->Unbind();
    textVAO->Unbind();
    ENGINE_PRINT("[Font] Successfully loaded font resource: ", path, " (size: ", fontSize,  ")\n");
    //std::cout << "[Font] Successfully loaded font resource: " << path << " (size: " << fontSize << ")" << std::endl;
    return true;
}

bool Font::ReloadResource(const std::string& assetPath)
{
    // When reloading fonts, don't reset the font size (keep the current font size).
    return LoadResource(assetPath, 0, false);
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
        LoadResource(fontAssetPath, newSize);
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

std::shared_ptr<AssetMeta> Font::ExtendMetaFile(const std::string& assetPath, std::shared_ptr<AssetMeta> currentMetaData)
{
    assetPath, currentMetaData;
    return std::shared_ptr<AssetMeta>();
}