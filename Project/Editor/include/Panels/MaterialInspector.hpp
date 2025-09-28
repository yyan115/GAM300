#pragma once

#include <memory>
#include <string>
#include <Graphics/Material.hpp>

// DLL export macro for editor
#ifdef _WIN32
    #ifdef EDITOR_EXPORTS
        #define EDITOR_API __declspec(dllexport)
    #else
        #define EDITOR_API __declspec(dllimport)
    #endif
#else
    #define EDITOR_API
#endif

class EDITOR_API MaterialInspector {
public:
    // GUI rendering method
    static void DrawMaterialAsset(std::shared_ptr<Material> material, const std::string& assetPath);
};