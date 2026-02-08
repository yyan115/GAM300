#include "pch.h"
#include "ECS/TagsLayersSettings.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include "ECS/SortingLayerManager.hpp"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include <fstream>
#include <filesystem>
#include <iostream>

TagsLayersSettings& TagsLayersSettings::GetInstance() {
    static TagsLayersSettings instance;
    return instance;
}

std::string TagsLayersSettings::GetSettingsFilePath(const std::string& projectPath) const {
    namespace fs = std::filesystem;

    fs::path basePath;
    if (projectPath.empty()) {
        // Use project root directory (parent of current working directory which is usually Build/EditorRelease)
        basePath = fs::current_path().parent_path().parent_path();
    } else {
        basePath = fs::path(projectPath);
    }

    fs::path settingsPath = basePath / settingsFolderName / settingsFileName;
    if (fs::exists(settingsPath))
        return settingsPath.string();
    else {
        // Fallback: Try looking at current path.
        basePath = fs::current_path();
        settingsPath = basePath / settingsFolderName / settingsFileName;
        return settingsPath.string();
    }
}

bool TagsLayersSettings::SaveSettings(const std::string& projectPath) {
    namespace fs = std::filesystem;

    // Prepare JSON document
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();

    // Serialize tags
    rapidjson::Value tagsArr(rapidjson::kArrayType);
    const auto& allTags = TagManager::GetInstance().GetAllTags();
    for (const auto& tag : allTags) {
        rapidjson::Value tagVal;
        tagVal.SetString(tag.c_str(), static_cast<rapidjson::SizeType>(tag.size()), alloc);
        tagsArr.PushBack(tagVal, alloc);
    }
    doc.AddMember("tags", tagsArr, alloc);

    // Serialize layers
    rapidjson::Value layersArr(rapidjson::kArrayType);
    const auto& allLayers = LayerManager::GetInstance().GetAllLayers();
    for (int i = 0; i < LayerManager::MAX_LAYERS; ++i) {
        if (!allLayers[i].empty()) {
            rapidjson::Value layerObj(rapidjson::kObjectType);
            layerObj.AddMember("index", i, alloc);
            rapidjson::Value nameVal;
            nameVal.SetString(allLayers[i].c_str(), static_cast<rapidjson::SizeType>(allLayers[i].size()), alloc);
            layerObj.AddMember("name", nameVal, alloc);
            layersArr.PushBack(layerObj, alloc);
        }
    }
    doc.AddMember("layers", layersArr, alloc);

    // Serialize sorting layers
    rapidjson::Value sortingLayersArr(rapidjson::kArrayType);
    const auto& allSortingLayers = SortingLayerManager::GetInstance().GetAllLayers();
    for (const auto& layer : allSortingLayers) {
        rapidjson::Value layerObj(rapidjson::kObjectType);
        layerObj.AddMember("id", layer.id, alloc);
        rapidjson::Value nameVal;
        nameVal.SetString(layer.name.c_str(), static_cast<rapidjson::SizeType>(layer.name.size()), alloc);
        layerObj.AddMember("name", nameVal, alloc);
        layerObj.AddMember("order", layer.order, alloc);
        sortingLayersArr.PushBack(layerObj, alloc);
    }
    doc.AddMember("sortingLayers", sortingLayersArr, alloc);

    // Get file path
    std::string filePath = GetSettingsFilePath(projectPath);
    fs::path fullPath(filePath);

    // Create directory if it doesn't exist
    fs::path parentDir = fullPath.parent_path();
    if (!fs::exists(parentDir)) {
        try {
            fs::create_directories(parentDir);
        } catch (const std::exception& e) {
            std::cerr << "[TagsLayersSettings] Failed to create directory: " << parentDir.string()
                      << " - " << e.what() << std::endl;
            return false;
        }
    }

    // Write to file
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        std::cerr << "[TagsLayersSettings] Failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    outFile << buffer.GetString();
    outFile.close();

    std::cout << "[TagsLayersSettings] Saved project settings to: " << filePath << std::endl;
    return true;
}

bool TagsLayersSettings::LoadSettings(const std::string& projectPath) {
    namespace fs = std::filesystem;

    std::string filePath = GetSettingsFilePath(projectPath);

    // Check if file exists
    if (!fs::exists(filePath)) {
        std::cout << "[TagsLayersSettings] Settings file not found, using defaults: " << filePath << std::endl;
        return false;
    }

    // Read file
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "[TagsLayersSettings] Failed to open file for reading: " << filePath << std::endl;
        return false;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();

    // Parse JSON
    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());

    if (doc.HasParseError()) {
        std::cerr << "[TagsLayersSettings] JSON parse error in: " << filePath << std::endl;
        return false;
    }

    // Load tags
    if (doc.HasMember("tags") && doc["tags"].IsArray()) {
        const auto& tagsArr = doc["tags"];
        // Clear existing tags (except first one which is default)
        auto& tagManager = TagManager::GetInstance();
        // Note: TagManager might not have a Clear method, so we keep defaults

        for (rapidjson::SizeType i = 0; i < tagsArr.Size(); ++i) {
            std::string tag = tagsArr[i].GetString();
            tagManager.AddTag(tag);
        }
    }

    // Load layers
    if (doc.HasMember("layers") && doc["layers"].IsArray()) {
        const auto& layersArr = doc["layers"];
        auto& layerManager = LayerManager::GetInstance();

        for (rapidjson::SizeType i = 0; i < layersArr.Size(); ++i) {
            const auto& layerObj = layersArr[i];
            if (layerObj.IsObject() && layerObj.HasMember("index") && layerObj.HasMember("name")) {
                int index = layerObj["index"].GetInt();
                std::string name = layerObj["name"].GetString();
                layerManager.SetLayerName(index, name);
            }
        }
    }

    // Load sorting layers
    if (doc.HasMember("sortingLayers") && doc["sortingLayers"].IsArray()) {
        auto& sortingLayerManager = SortingLayerManager::GetInstance();
        sortingLayerManager.Clear(); // Clear and reload from settings

        const auto& sortingLayersArr = doc["sortingLayers"];
        for (rapidjson::SizeType i = 0; i < sortingLayersArr.Size(); ++i) {
            const auto& layerObj = sortingLayersArr[i];
            if (layerObj.IsObject() && layerObj.HasMember("name")) {
                std::string name = layerObj["name"].GetString();
                sortingLayerManager.AddLayer(name);
            }
        }
    }

    std::cout << "[TagsLayersSettings] Loaded project settings from: " << filePath << std::endl;
    return true;
}
