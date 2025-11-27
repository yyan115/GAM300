#pragma once
#include <string>
#include "Utilities/GUID.hpp"
#include "../Engine.h"

class MetaFilesManager {
public:
	// ENGINE_API static GUID_128 GenerateMetaFile(const std::string& assetPath, const std::string& resourcePath);

    ENGINE_API static bool MetaFileExists(const std::string& assetPath);

    //static std::chrono::system_clock::time_point GetLastCompileTimeFromMetaFile(const std::string& metaFilePath);

    static GUID_string GetGUIDFromMetaFile(const std::string& metaFilePath);

	static GUID_string GetGUIDFromAssetFile(const std::string& assetPath);

	ENGINE_API static void InitializeAssetMetaFiles(const std::string& rootAssetFolder);

    ENGINE_API static GUID_128 GetGUID128FromAssetFile(const std::string& assetPath);

    ENGINE_API static std::string GetResourceNameFromAssetFile(const std::string& assetPath);

    ENGINE_API static bool MetaFileUpdated(const std::string& assetPath);

    //static bool AssetFileUpdated(const std::string& assetPath);

	// ENGINE_API static GUID_128 UpdateMetaFile(const std::string& assetPath);

	static void AddGUID128Mapping(const std::string& assetPath, const GUID_128& guid);

    ENGINE_API static bool DeleteMetaFile(const std::string& assetPath);

    ENGINE_API static void CleanupUnusedMetaFiles();

    static constexpr int CURRENT_METADATA_VERSION = 7;

private:
    /**
    * \brief Deleted default constructor to prevent instantiation.
    */
    MetaFilesManager() = delete;

    /**
     * \brief Deleted destructor to prevent instantiation.
     */
    ~MetaFilesManager() = delete;

    static std::unordered_map<std::string, GUID_128> assetPathToGUID128; // Map from an asset's file path to its GUID_128_t value.

};