#pragma once
#include <string>
#include "Engine.h"

/**
 * @brief Manages project-wide settings for tags, layers, and sorting layers.
 *
 * This class handles saving and loading tags, layers, and sorting layers
 * from a project settings file. These settings are shared across all scenes.
 */
class ENGINE_API TagsLayersSettings {
public:
    static TagsLayersSettings& GetInstance();

    /**
     * @brief Save all tags, layers, and sorting layers to the project settings file.
     * @param projectPath The root path of the project (default uses Assets folder)
     * @return True if saved successfully, false otherwise.
     */
    bool SaveSettings(const std::string& projectPath = "");

    /**
     * @brief Load all tags, layers, and sorting layers from the project settings file.
     * @param projectPath The root path of the project (default uses Assets folder)
     * @return True if loaded successfully, false otherwise.
     */
    bool LoadSettings(const std::string& projectPath = "");

    /**
     * @brief Get the default settings file path.
     * @param projectPath The root path of the project
     * @return The full path to the settings file.
     */
    std::string GetSettingsFilePath(const std::string& projectPath = "") const;

private:
    TagsLayersSettings() = default;
    ~TagsLayersSettings() = default;
    TagsLayersSettings(const TagsLayersSettings&) = delete;
    TagsLayersSettings& operator=(const TagsLayersSettings&) = delete;

    const std::string settingsFileName = "TagsAndLayers.json";
    const std::string settingsFolderName = "ProjectSettings";
};
