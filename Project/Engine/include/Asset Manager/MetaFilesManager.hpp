#pragma once
#include <string>
#include "GUID.hpp"

#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
// Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

class MetaFilesManager {
public:
	// ENGINE_API static GUID_128 GenerateMetaFile(const std::string& assetPath, const std::string& resourcePath);

    ENGINE_API static bool MetaFileExists(const std::string& assetPath);

    static GUID_string GetGUIDFromMetaFile(const std::string& metaFilePath);

	static GUID_string GetGUIDFromAssetFile(const std::string& assetPath);

	static void InitializeAssetMetaFiles(const std::string& rootAssetFolder);

    ENGINE_API static GUID_128 GetGUID128FromAssetFile(const std::string& assetPath);

    ENGINE_API static bool MetaFileUpdated(const std::string& assetPath);

	// ENGINE_API static GUID_128 UpdateMetaFile(const std::string& assetPath);

	static void AddGUID128Mapping(const std::string& assetPath, const GUID_128& guid);

	static bool DeleteMetaFile(const std::string& assetPath);

    static constexpr int CURRENT_METADATA_VERSION = 3;

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