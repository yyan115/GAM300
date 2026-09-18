// AssetCooker — compiles source art into the runtime formats the game loads.
//
// The game reads .dds textures and .mesh models. Those are build output: the
// editor produces them, they are gitignored, and a clean CI checkout has none,
// so an installer built there ships a game that cannot load a single texture.
// This makes the same compilation runnable from the command line, so CI can do
// it as a build step instead of depending on what happens to be on a
// developer's disk.
//
//   AssetCooker --resources <dir> [--android] [--verbose]
//
// Exits non-zero if any asset fails to compile, so a broken cook fails the
// build rather than silently shipping an incomplete set.
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/MetaFilesManager.hpp"
#include "WindowManager.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    std::string resources = "Resources";
    bool android = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--resources" && i + 1 < argc) {
            resources = argv[++i];
        } else if (arg == "--android") {
            android = true;
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "AssetCooker --resources <dir> [--android] [--verbose]\n";
            return 0;
        } else {
            std::cerr << "AssetCooker: unknown argument '" << arg << "'\n";
            return 2;
        }
    }

    if (!fs::exists(resources)) {
        std::cerr << "AssetCooker: resources directory not found: " << resources << "\n";
        return 2;
    }

    const fs::path root = fs::absolute(resources);
    std::cout << "AssetCooker: cooking " << root.generic_string()
              << (android ? " for Android\n" : " for desktop\n");

    // Asset discovery goes through the platform abstraction, so a platform has
    // to exist. Deliberately not WindowManager::Init, which would also open a
    // window and a GL context and fail on a machine with no display.
    if (!WindowManager::InitPlatformOnly()) {
        std::cerr << "AssetCooker: could not create the platform\n";
        return 2;
    }

    AssetManager& assets = AssetManager::GetInstance();

    // This walks the asset tree, registers GUIDs and meta files, and compiles
    // anything whose output is missing or older than its source. Doing the walk
    // by hand instead does not work: CompileAsset needs the GUID and meta
    // registration this does first, and without it every asset fails.
    //
    // It also tries to compile shaders, which is GL work. Shader::SetupShader
    // now refuses when there is no GL context rather than dereferencing a null
    // function pointer, so that part fails harmlessly here.
    MetaFilesManager::InitializeAssetMetaFiles(root.generic_string());

    // Then force every asset. The registration above is necessary but not
    // sufficient: CompileAssetToResource decides with
    //   shouldCompile = forceCompile || asset not already in the map
    // which never looks at whether the compiled file exists. On a clean
    // checkout the .meta files are present and say the asset was compiled,
    // while the .dds and .mesh they refer to are gitignored and absent, so
    // without forcing nothing is produced at all.
    //
    // Shaders are skipped: linking one is GL work and there is no context here.
    size_t cooked = 0, failed = 0, shaders = 0;
    std::vector<std::string> failures;

    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;

        const fs::path& file = entry.path();
        const std::string ext = file.extension().generic_string();
        if (ext == ".meta") continue;
        if (!assets.IsAssetExtensionSupported(ext)) continue;
        if (assets.IsExtensionShaderVertFrag(ext)) { ++shaders; continue; }

        const std::string path = file.generic_string();
        if (assets.CompileAsset(path, true, android)) {
            ++cooked;
            if (verbose) std::cout << "  cooked: " << path << "\n";
        } else {
            ++failed;
            if (failures.size() < 20) failures.push_back(path);
        }
    }

    std::cout << "AssetCooker: " << cooked << " compiled, " << failed
              << " failed, " << shaders << " shaders skipped\n";
    for (const std::string& f : failures) std::cerr << "  failed: " << f << "\n";

    // Count what actually landed on disk, because "the call returned true" and
    // "the file exists" are not the same claim. An Android cook writes into the
    // Android project, as .ktx textures, rather than next to its sources.
    const fs::path outputRoot = android ? assets.GetAndroidResourcesPath() : root;
    const std::string textureExt = android ? ".ktx" : ".dds";
    size_t textures = 0, mesh = 0, font = 0;
    std::error_code walkError;
    for (fs::recursive_directory_iterator it(outputRoot, walkError), end; it != end; it.increment(walkError)) {
        if (walkError) break;
        if (!it->is_regular_file()) continue;
        const std::string ext = it->path().extension().generic_string();
        if (ext == textureExt) ++textures;
        else if (ext == ".mesh") ++mesh;
        else if (ext == ".font") ++font;
    }

    std::cout << "AssetCooker: on disk now - " << textures << " " << textureExt << ", " << mesh
              << " .mesh, " << font << " .font\n";

    // The compile calls happen inside InitializeAssetMetaFiles and it reports no
    // tally, so the check is what landed on disk. Zero of anything means the
    // cook did not work, and shipping that produces a game with no textures.
    if (textures == 0 || mesh == 0) {
        std::cerr << "AssetCooker: nothing was produced (" << textureExt << "=" << textures
                  << ", .mesh=" << mesh << ")\n";
        return 1;
    }
    return 0;
}
