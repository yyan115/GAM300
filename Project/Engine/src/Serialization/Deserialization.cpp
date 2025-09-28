#include "pch.h"
#include "Serialization/Deserialization.hpp"

//#include "../config.hpp"
//#include "../ECS/ECS.hpp"
//#include "../AssetManager/AssetManager.hpp"
//#include "../GameEngine/Logger.hpp"

#if 0

extern Logger logger;

extern std::atomic<bool> gameEngineShuttingDown;

namespace rj = rapidjson;

namespace detail {

    // ---------- File I/O helpers ----------

    inline bool loadDoc(const std::string& path, rj::Document& doc) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return false;
        rj::IStreamWrapper isw(ifs);
        doc.ParseStream(isw);
        return !doc.HasParseError();
    }

    template <typename RJValue>
    inline bool writePretty(const std::string& path, RJValue& v) {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) return false;
        rj::OStreamWrapper osw(ofs);
        rj::PrettyWriter<rj::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 2);
        return v.Accept(writer);
    }

    // ---------- JSON helpers ----------

    inline const rj::Value* get(const rj::Value& obj, const char* key) {
        if (!obj.IsObject()) return nullptr;
        auto it = obj.FindMember(key);
        if (it == obj.MemberEnd()) return nullptr;
        return &it->value;
    }

    inline rj::Value makeString(const std::string& s, rj::Document::AllocatorType& a) {
        rj::Value v;
        v.SetString(s.c_str(), static_cast<rj::SizeType>(s.size()), a);
        return v;
    }

} // namespace detail


// ========================================================================================
// Local utilities
// ========================================================================================

namespace {

    void processSubFolderRJ(
        rj::Value const& folders,
        const char* assetType,
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<AssetID>>>& subFolders
    ) {
        auto [it, _] = subFolders.insert({ assetType, {} });
        auto m = folders.FindMember(assetType);
        if (m == folders.MemberEnd() || !m->value.IsArray()) return;

        for (auto const& entry : m->value.GetArray()) {
            if (!entry.IsObject()) continue;
            std::string subFolderName = entry["name"].GetString();
            auto& bucket = it->second[subFolderName];
            if (entry.HasMember("ids") && entry["ids"].IsArray()) {
                for (auto const& idv : entry["ids"].GetArray()) {
                    if (idv.IsUint()) bucket.push_back(idv.GetUint());
                }
            }
        }
    }

    AudioSettings deserialiseAudioConfig(std::string const& audioFileName) {
        AudioSettings audioSettings{ 1.f, 1.f };

        // https://stackoverflow.com/questions/2812760/print-tchar-on-console
        TCHAR my_documents[MAX_PATH];
        HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);

        if (result != S_OK) {
            logger.error(std::string("Error attempting to retrieve documents directory.. HRESULT = ") + std::to_string(result));
            return audioSettings;
        }

        try {
            // Get documents file directory
            std::filesystem::path documentPath{ my_documents };
            documentPath /= audioFileName;

            logger.log("Documents path detected: " + documentPath.string());

            // Check if audio config exists..
            if (!std::filesystem::exists(documentPath)) {
                // first time, create this json..
                logger.log("Audio config does not exist, probably first installation. Creating it.. " + documentPath.string());
                serialiseConfig(audioFileName, 1.f, 1.f);
                return audioSettings;
            }
            else {
                // parse this json...
                rj::Document doc;
                if (!detail::loadDoc(documentPath.string(), doc)) {
                    std::cout << "Something went wrong when opening/parsing: " << audioFileName << "\n";
                    return audioSettings;
                }

                if (auto* v = detail::get(doc, "BGM"); v && v->IsNumber()) audioSettings.bgm = v->GetFloat();
                if (auto* v = detail::get(doc, "SFX"); v && v->IsNumber()) audioSettings.sfx = v->GetFloat();

                logger.log("Successfully deserialised audio config. :)");

                return audioSettings;
            }
        }
        catch (std::exception const& ex) {
            std::cout << "Exception caught: " << ex.what() << "\n";
        }

        return audioSettings;
    }

} // anonymous namespace


// ========================================================================================
// Config (de)serialisation
// ========================================================================================

std::tuple<WindowSettings, GameSettings, GraphicsSettings, UISettings, AudioSettings, LevelEditorSettings>
deserialiseConfig(std::string const& filename, std::string const& audioFileName) {
    rj::Document doc;
    if (!detail::loadDoc(filename, doc)) {
        throw std::runtime_error("Invalid json filepath or parse error: " + filename + ", or " + audioFileName);
    }

    // window
    WindowSettings windowSettings{};
    if (auto const* window = detail::get(doc, "window")) {
        if (auto* v = detail::get(*window, "title");      v && v->IsString()) windowSettings.title = v->GetString();
        if (auto* v = detail::get(*window, "width");      v && v->IsInt())    windowSettings.windowWidth = v->GetInt();
        if (auto* v = detail::get(*window, "height");     v && v->IsInt())    windowSettings.windowHeight = v->GetInt();
        if (auto* v = detail::get(*window, "fullscreen"); v && v->IsBool())   windowSettings.fullscreen = v->GetBool();
        if (auto* v = detail::get(*window, "maxFps");     v && v->IsNumber()) windowSettings.maxFps = static_cast<float>(v->GetDouble());
        if (auto* v = detail::get(*window, "fixedFps");   v && v->IsNumber()) windowSettings.fixedDeltaFps = static_cast<float>(v->GetDouble());
        if (auto* v = detail::get(*window, "vSync");      v && v->IsBool())   windowSettings.vsync = v->GetBool();
        if (auto* v = detail::get(*window, "maxNumOfSteps"); v && v->IsInt()) windowSettings.maxNumOfSteps = v->GetInt();
    }

    // game
    GameSettings gameSettings{};
    if (auto const* game = detail::get(doc, "game")) {
        if (auto* v = detail::get(*game, "title");  v && v->IsString()) gameSettings.title = v->GetString();
        if (auto* v = detail::get(*game, "width");  v && v->IsInt())    gameSettings.width = v->GetInt();
        if (auto* v = detail::get(*game, "height"); v && v->IsInt())    gameSettings.height = v->GetInt();
    }

    // graphics
    GraphicsSettings graphicsSettings{};
    if (auto const* graphics = detail::get(doc, "graphics")) {
        if (auto* v = detail::get(*graphics, "drawLimit");         v && v->IsUint())  graphicsSettings.drawLimit = v->GetUint();
        if (auto* v = detail::get(*graphics, "particlesEnabled");  v && v->IsBool())  graphicsSettings.particlesEnabled = v->GetBool();
        if (auto* v = detail::get(*graphics, "maxNumOfParticles"); v && v->IsUint())  graphicsSettings.maxNumOfParticles = v->GetUint();
    }

    // ui
    UISettings uiSettings{};
    if (auto const* ui = detail::get(doc, "ui")) {
        if (auto* v = detail::get(*ui, "width");  v && v->IsInt()) uiSettings.width = v->GetInt();
        if (auto* v = detail::get(*ui, "height"); v && v->IsInt()) uiSettings.height = v->GetInt();
    }

#ifndef DISABLE_IMGUI_LEVELEDITOR
    LevelEditorSettings levelEditorSettings{};
    if (auto const* le = detail::get(doc, "levelEditor")) {
        if (auto* v = detail::get(*le, "font");     v && v->IsString()) levelEditorSettings.fontFilePath = v->GetString();
        if (auto* v = detail::get(*le, "fontSize"); v && v->IsNumber()) levelEditorSettings.fontSize = static_cast<float>(v->GetDouble());
    }

    return std::make_tuple(windowSettings, gameSettings, graphicsSettings, uiSettings, deserialiseAudioConfig(audioFileName), levelEditorSettings);
#else
    return std::make_tuple(windowSettings, gameSettings, graphicsSettings, uiSettings, deserialiseAudioConfig(audioFileName), LevelEditorSettings{});
#endif
}

void serialiseConfig(std::string const& audioFileName, float bgmPercentage, float sfxPercentage) {
    // https://stackoverflow.com/questions/2812760/print-tchar-on-console
    TCHAR my_documents[MAX_PATH];
    HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);

    if (result != S_OK) {
        logger.error(std::string("Error attempting to retrieve documents directory.. HRESULT = ") + std::to_string(result));
        return;
    }

    try {
        // Get documents file directory
        std::filesystem::path documentPath{ my_documents };
        documentPath /= audioFileName;

        rj::Document doc(rj::kObjectType);
        auto& a = doc.GetAllocator();

        doc.AddMember("BGM", bgmPercentage, a);
        doc.AddMember("SFX", sfxPercentage, a);

        if (!detail::writePretty(documentPath.string(), doc)) {
            logger.error("Error when creating/writing audio config file..");
        }
    }
    catch (std::exception const& ex) {
        ENGINE_PRINT(
        std::cout << "Exception caught: " << ex.what() << "\n";
    }
}

// Overload: write the main config (window/game/graphics/ui/levelEditor)
void serialiseConfig(const std::string& filename,
    const WindowSettings& windowSettings,
    const GameSettings& gameSettings,
    const GraphicsSettings& graphicsSettings,
    const UISettings& uiSettings,
    const LevelEditorSettings& levelEditorSettings)
{
    rj::Document doc(rj::kObjectType);
    auto& a = doc.GetAllocator();

    // Window
    {
        rj::Value o(rj::kObjectType);
        o.AddMember("title", detail::makeString(windowSettings.title, a), a);
        o.AddMember("width", windowSettings.windowWidth, a);
        o.AddMember("height", windowSettings.windowHeight, a);
        o.AddMember("fullscreen", windowSettings.fullscreen, a);
        o.AddMember("maxFps", windowSettings.maxFps, a);
        o.AddMember("fixedFps", windowSettings.fixedDeltaFps, a);
        o.AddMember("vSync", windowSettings.vsync, a);
        o.AddMember("maxNumOfSteps", windowSettings.maxNumOfSteps, a);
        doc.AddMember("window", o, a);
    }

    // Game
    {
        rj::Value o(rj::kObjectType);
        o.AddMember("title", detail::makeString(gameSettings.title, a), a);
        o.AddMember("width", gameSettings.width, a);
        o.AddMember("height", gameSettings.height, a);
        doc.AddMember("game", o, a);
    }

    // Graphics
    {
        rj::Value o(rj::kObjectType);
        o.AddMember("drawLimit", graphicsSettings.drawLimit, a);
        o.AddMember("particlesEnabled", graphicsSettings.particlesEnabled, a);
        o.AddMember("maxNumOfParticles", graphicsSettings.maxNumOfParticles, a);
        doc.AddMember("graphics", o, a);
    }

    // UI
    {
        rj::Value o(rj::kObjectType);
        o.AddMember("width", uiSettings.width, a);
        o.AddMember("height", uiSettings.height, a);
        doc.AddMember("ui", o, a);
    }

#ifndef DISABLE_IMGUI_LEVELEDITOR
    // Level Editor (if enabled)
    {
        rj::Value o(rj::kObjectType);
        o.AddMember("font", detail::makeString(levelEditorSettings.fontFilePath, a), a);
        o.AddMember("fontSize", levelEditorSettings.fontSize, a);
        doc.AddMember("levelEditor", o, a);
    }
#endif

    if (!detail::writePretty(filename, doc)) {
        logger.error("Failed to open file to serialise config!");
    }
}


// ========================================================================================
// Assets metadata (de)serialisation
// ========================================================================================

void deserialiseAssetMetadata(ECS& registry, AssetManager& assetManager, std::string const& filename) {
    rj::Document doc;
    if (!detail::loadDoc(filename, doc)) {
        logger.error("Failed to open/parse JSON file '" + filename + "' for asset deserialisation!\n");
        return;
    }

    auto* assets = detail::get(doc, "assets");
    if (!assets || !assets->IsArray()) return;

    for (auto& asset : assets->GetArray()) {
        if (!asset.IsObject()) continue;
        for (auto it = asset.MemberBegin(); it != asset.MemberEnd(); ++it) {
            std::string type = it->name.GetString();
            const rj::Value& arr = it->value;
            if (!arr.IsArray()) continue;

            if (type == "Animation") {
                for (auto& anim : arr.GetArray()) {
                    AssetID id = anim["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadAnimation(
                        anim["name"].GetString(),
                        anim["filepath"].GetString(),
                        anim["rows"].GetUint(),
                        anim["cols"].GetUint(),
                        anim["frames"].GetUint(),
                        static_cast<float>(anim["animTime"].GetDouble()),
                        id
                    );
                }
            }
            else if (type == "Audio") {
                for (auto& au : arr.GetArray()) {
                    AssetID id = au["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadAudio(
                        au["name"].GetString(),
                        au["filepath"].GetString(),
                        id
                    );
                }
            }
            else if (type == "Font") {
                for (auto& f : arr.GetArray()) {
                    AssetID id = f["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadFont(
                        f["name"].GetString(),
                        f["filepath"].GetString(),
                        id
                    );
                }
            }
            else if (type == "Image") {
                for (auto& im : arr.GetArray()) {
                    AssetID id = im["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadImage(
                        im["name"].GetString(),
                        im["filepath"].GetString(),
                        id
                    );
                }
            }
            else if (type == "Prefab") {
                for (auto& p : arr.GetArray()) {
                    AssetID id = p["id"].GetUint();
                    assetManager.insertUsedId(id);

                    if (deserialisePrefab(registry, p["name"].GetString(), p["filepath"].GetString(), id)) {
                        assetManager.loadPrefabFile(
                            p["name"].GetString(),
                            p["filepath"].GetString(),
                            id
                        );
                    }
                }
            }
            else if (type == "Scene") {
                for (auto& s : arr.GetArray()) {
                    AssetID id = s["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadSceneFile(
                        s["name"].GetString(),
                        s["filepath"].GetString(),
                        id,
                        s["startWithThisScene"].GetBool()
                    );
                }
            }
            else if (type == "Script") {
                for (auto& sc : arr.GetArray()) {
                    AssetID id = sc["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadScriptFile(
                        sc["name"].GetString(),
                        sc["filepath"].GetString(),
                        id
                    );
                }
            }
            else if (type == "Shader") {
                for (auto& sh : arr.GetArray()) {
                    AssetID id = sh["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadShaderFile(
                        sh["name"].GetString(),
                        sh["filepath"].GetString(),
                        id
                    );
                }
            }
            else if (type == "Video") {
                for (auto& v : arr.GetArray()) {
                    AssetID id = v["id"].GetUint();
                    assetManager.insertUsedId(id);

                    assetManager.loadVideo(
                        v["name"].GetString(),
                        v["filepath"].GetString(),
                        id
                    );
                }
            }
        }
    }
}

#ifndef DISABLE_IMGUI_LEVELEDITOR
void serialiseAssetMetadata(AssetManager& assetManager, std::string const& filename) {
    rj::Document doc(rj::kObjectType);
    auto& a = doc.GetAllocator();

    rj::Value outer(rj::kArrayType);

    auto addBucket = [&](const char* typeName, auto&& range, auto&& fillFn) {
        rj::Value arr(rj::kArrayType);
        for (auto const& [id, item] : range) {
            rj::Value o(rj::kObjectType);
            fillFn(o, id, item, a);
            arr.PushBack(o, a);
        }
        rj::Value wrapper(rj::kObjectType);
        wrapper.AddMember(detail::makeString(typeName, a), arr, a);
        outer.PushBack(wrapper, a);
        };

    addBucket("Animation", assetManager.getAnimations(),
        [](rj::Value& o, AssetID id, auto const& anim, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(anim.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(anim.getFilepath(), a), a);
            o.AddMember("rows", anim.getRows(), a);
            o.AddMember("cols", anim.getCols(), a);
            o.AddMember("frames", anim.getTotalFrames(), a);
            o.AddMember("animTime", anim.getAnimationTime(), a);
        });

    addBucket("Audio", assetManager.getAudios(),
        [](rj::Value& o, AssetID id, auto const& audio, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(audio.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(audio.getFilepath(), a), a);
        });

    addBucket("Font", assetManager.getFonts(),
        [](rj::Value& o, AssetID id, auto const& font, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(font.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(font.getFilepath(), a), a);
        });

    addBucket("Image", assetManager.getImages(),
        [](rj::Value& o, AssetID id, auto const& image, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(image.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(image.getFilepath(), a), a);
        });

    addBucket("Prefab", assetManager.getPrefabFiles(),
        [](rj::Value& o, AssetID id, auto const& pf, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(pf.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(pf.getFilepath(), a), a);
        });

    addBucket("Scene", assetManager.getSceneFiles(),
        [](rj::Value& o, AssetID id, auto const& sc, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(sc.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(sc.getFilepath(), a), a);
            o.AddMember("startWithThisScene", sc.getStartWithThisScene(), a);
        });

    addBucket("Shader", assetManager.getShaderFiles(),
        [](rj::Value& o, AssetID id, auto const& sh, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(sh.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(sh.getFilepath(), a), a);
        });

    addBucket("Script", assetManager.getScriptFiles(),
        [](rj::Value& o, AssetID id, auto const& sc, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(sc.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(sc.getFilepath(), a), a);
        });

    addBucket("Video", assetManager.getVideos(),
        [](rj::Value& o, AssetID id, auto const& v, rj::Document::AllocatorType& a) {
            o.AddMember("id", id, a);
            o.AddMember("name", detail::makeString(v.getKey(), a), a);
            o.AddMember("filepath", detail::makeString(v.getFilepath(), a), a);
        });

    doc.AddMember("assets", outer, a);
    if (!detail::writePretty(filename, doc)) {
        logger.error("Failed to open JSON file '" + filename + "' for asset serialisation!\n");
    }
}
#endif // !DISABLE_IMGUI_LEVELEDITOR


// ========================================================================================
// Asset folders (de)serialisation
// ========================================================================================

std::unordered_map<
    std::string,
    std::unordered_map<std::string, std::vector<AssetID>>
> deserialiseAssetFolders(std::string const& filename) {
    rj::Document doc;
    if (!detail::loadDoc(filename, doc)) {
        logger.error("Failed to open JSON file '" + filename + "' for asset deserialisation!\n");
        return {};
    }

    const rj::Value* folders = detail::get(doc, "folders");
    if (!folders || !folders->IsObject()) {
        logger.error("Error parsing JSON '" + filename + "' for asset folders (missing 'folders' object)!\n");
        return {};
    }

    std::unordered_map<
        std::string,
        std::unordered_map<std::string, std::vector<AssetID>>
    > subFolders;

    processSubFolderRJ(*folders, "Animation", subFolders);
    processSubFolderRJ(*folders, "Audio", subFolders);
    processSubFolderRJ(*folders, "Video", subFolders);
    processSubFolderRJ(*folders, "Font", subFolders);
    processSubFolderRJ(*folders, "Image", subFolders);
    processSubFolderRJ(*folders, "Prefab", subFolders);
    processSubFolderRJ(*folders, "Scene", subFolders);
    processSubFolderRJ(*folders, "Script", subFolders);
    processSubFolderRJ(*folders, "Shader", subFolders);

    return subFolders;
}

#ifndef DISABLE_IMGUI_LEVELEDITOR
void serialiseAssetFolders(
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<unsigned int>>> const assetSubFolders,
    std::string const& filename
) {
    rj::Document doc(rj::kObjectType);
    auto& a = doc.GetAllocator();

    // Wrap everything under a top-level "folders" object
    rj::Value folders(rj::kObjectType);

    auto build = [&](const char* key) {
        rj::Value arr(rj::kArrayType);
        if (auto it = assetSubFolders.find(key); it != assetSubFolders.end()) {
            for (auto const& [sub, ids] : it->second) {
                rj::Value obj(rj::kObjectType);
                obj.AddMember("name", detail::makeString(sub, a), a);

                rj::Value idArr(rj::kArrayType);
                for (auto id : ids) idArr.PushBack(id, a);
                obj.AddMember("ids", idArr, a);

                arr.PushBack(obj, a);
            }
        }
        folders.AddMember(detail::makeString(key, a), arr, a);
        };

    build("Animation"); build("Audio"); build("Font"); build("Image");
    build("Prefab");   build("Scene"); build("Shader"); build("Script"); build("Video");

    doc.AddMember("folders", folders, a);

    if (!detail::writePretty(filename, doc)) {
        logger.error("Failed to open JSON file '" + filename + "' for asset serialisation!\n");
    }
}
#endif // !DISABLE_IMGUI_LEVELEDITOR


// ========================================================================================
// Scene / Prefab (de)serialisation
// ========================================================================================

void preloadAssets(std::string const& name, AssetManager& assetManager)
{
    auto it = std::find_if(
        assetManager.getSceneFiles().begin(), assetManager.getSceneFiles().end(),
        [&](std::pair<const AssetID, SceneFile> const& pair) {
            return pair.second.getKey() == name;
        }
    );

    if (it == assetManager.getSceneFiles().end()) {
        throw std::runtime_error("Invalid scene name passed in Lua function");
    }

    // Already loaded, dont need to do anything further
    if (it->second.checkFinishedPreload()) return;

    std::string filename = it->second.getFilepath();

    rj::Document doc;
    if (!detail::loadDoc(filename, doc)) {
        logger.error("Failed to open JSON file '" + filename + "' to load assets!\n");
        return;
    }

    if (auto* ids = detail::get(doc, "assetIds"); ids && ids->IsArray()) {
        for (auto const& v : ids->GetArray()) {
            // game engine is shutting down..
            if (gameEngineShuttingDown) {
                return;
            }
            if (v.IsUint()) assetManager.lazyLoad(v.GetUint());
        }
    }

    // Set flag to true
    it->second.finishPreload();
}

void deserialiseScene(ECS& registry, std::string const& filename, AssetManager& assetManager) {

    rj::Document doc;
    if (!detail::loadDoc(filename, doc)) {
        logger.error("Failed to open JSON file '" + filename + "' for scene deserialisation!\n");
        return;
    }

    auto it = std::find_if(
        assetManager.getSceneFiles().begin(), assetManager.getSceneFiles().end(),
        [&](std::pair<const AssetID, SceneFile> const& pair) {
            return pair.second.getFilepath() == filename;
        }
    );

    // If assets were not already preloaded, start loading them now
    // Better late than never :)
    if (it != assetManager.getSceneFiles().end() && !(it->second.checkFinishedPreload())) {
        preloadAssets(it->second.getKey(), assetManager);
    }

    registry.deserializeScene(doc);
}

bool deserialisePrefab(ECS& registry, std::string const& key, std::string const& filename, AssetID assetId) {
    rj::Document doc;
    if (!detail::loadDoc(filename, doc)) {
        logger.error("Failed to open JSON file '" + filename + "' for prefab deserialisation!\n");
        return false;
    }

    Prefab& prefab = registry.createPrefab(key, assetId);
    registry.deserializePrefab(prefab, doc);
    return true;
}

#ifndef DISABLE_IMGUI_LEVELEDITOR
void serialiseScenes(ECS const& registry, AssetManager& assetManager) {
    auto const& sceneFiles = assetManager.getSceneFiles();

    for (auto const& [id, scene] : registry.getScenes()) {
        // Get filepath by matching with AssetManager
        auto it = std::find_if(sceneFiles.begin(), sceneFiles.end(),
            [&](std::pair<const AssetID, SceneFile> const& sceneFileIt) {
                return sceneFileIt.second.getSceneId() == id;
            });

        if (it == sceneFiles.end()) continue;

        // Expecting registry.serializeScene(scene, alloc) -> rapidjson::Value
        rj::Document doc(rj::kObjectType);
        auto& a = doc.GetAllocator();

        rj::Value out = registry.serializeScene(scene, a); // adapt to your signature
        doc.Swap(out);

        if (!detail::writePretty(it->second.getFilepath(), doc)) {
            logger.error("Failed to write scene JSON: " + it->second.getFilepath());
        }
    }
}

void serialiseScene(ECS const& registry, SceneFile const& sceneFile) {
    if (sceneFile.getId() == NO_SCENEID_ATTACHED) {
        logger.error("Failed to open file to serialise scene! (No scene ID)");
        return;
    }

    auto it = std::find_if(registry.getScenes().begin(), registry.getScenes().end(),
        [&](std::pair<const SceneID, ECS::Scene> const& scenePair) {
            return sceneFile.getSceneId() == scenePair.first;
        });

    if (it == registry.getScenes().end()) {
        logger.error("Scene not found in registry for serialisation.");
        return;
    }

    rj::Document doc(rj::kObjectType);
    auto& a = doc.GetAllocator();

    rj::Value out = registry.serializeScene(it->second, a); // adapt to your signature
    doc.Swap(out);

    if (!detail::writePretty(sceneFile.getFilepath(), doc)) {
        logger.error("Failed to open file to serialise scene!");
    }
}

std::vector<AssetID> serialisePrefab(ECS& registry, AssetManager& assetManager, AssetID id) {
    PrefabFile& prefabFile = assetManager.getPrefabFile(id);

    Prefab* prefab = registry.getPrefabFromAsset(id);
    if (!prefab) return {};

    // Expecting: registry.serializePrefab(*prefab) -> (rapidjson::Value, std::vector<AssetID>)
    auto [jsonVal, assetIds] = registry.serializePrefab(*prefab);

    rj::Document doc(rj::kObjectType);
    doc.Swap(jsonVal);

    if (!detail::writePretty(prefabFile.getFilepath(), doc)) {
        logger.error("Failed to open file to serialize prefab!");
        return {};
    }
    return assetIds;
}
#endif // !DISABLE_IMGUI_LEVELEDITOR

#endif