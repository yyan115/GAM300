#include "pch.h"

#include <iostream>
#include "Asset Manager/FileUtilities.hpp"
#include "Logging.hpp"

bool FileUtilities::RemoveFile(const std::string& filePath) {
	std::filesystem::path p(filePath);
	if (std::filesystem::exists(p) == false) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] WARNING: Attempted to delete non-existent file: ", p.generic_string(), "\n");
		//std::cerr << "[FileUtilities] WARNING: Attempted to delete non-existent file: " << p.generic_string() << std::endl;
		return true; // Consider it a success since the file doesn't exist.
	}

	std::error_code ec;
	bool success = std::filesystem::remove(p, ec);
	if (!success || ec) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] ERROR: Failed to delete file: " , p.generic_string() , " (" , ec.message() , ")\n");
		//std::cerr << "[FileUtilities] ERROR: Failed to delete file: " << p.generic_string() << " (" << ec.message() << ")" << std::endl;
		return false;
	}
	ENGINE_PRINT("[FileUtilities] Successfully deleted file: " , p.generic_string(), "\n");
	//std::cout << "[FileUtilities] Successfully deleted file: " << p.generic_string() << std::endl;
	return true;
}