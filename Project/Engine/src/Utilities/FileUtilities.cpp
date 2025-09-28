#include "pch.h"

#include <iostream>
#include "Utilities/FileUtilities.hpp"
#include "Logging.hpp"

std::filesystem::path FileUtilities::solutionRootDir;

bool FileUtilities::RemoveFile(const std::string& filePath) {
	// Remove the file from the current build folder first.
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

	std::cout << "[FileUtilities] Successfully deleted file: " << p.generic_string() << std::endl;

	// Then remove the file from the solution root folder to update all future builds.
	return RemoveFromSolutionRootDir(filePath);
}

const std::filesystem::path& FileUtilities::GetSolutionRootDir() {
	if (solutionRootDir.empty()) {
		auto dir = std::filesystem::current_path();
		bool found = false;
		while (!dir.empty()) {
			for (auto& entry : std::filesystem::directory_iterator(dir)) {
				if (entry.path().extension() == ".sln") {
					solutionRootDir = dir;
					found = true;
					break;
				}
			}

			if (found)
				break;

			dir = dir.parent_path();
		}
	}

	return solutionRootDir;
}

bool FileUtilities::RemoveFromSolutionRootDir(const std::string& filePath) {
	std::filesystem::path p(GetSolutionRootDir() / filePath);
	if (std::filesystem::exists(p) == false) {
		std::cerr << "[FileUtilities] WARNING: Attempted to delete non-existent file: " << p.generic_string() << std::endl;
		return true; // Consider it a success since the file doesn't exist.
	}

	std::error_code ec;
	bool success = std::filesystem::remove(p, ec);
	if (!success || ec) {
		std::cerr << "[FileUtilities] ERROR: Failed to delete file: " << p.generic_string() << " (" << ec.message() << ")" << std::endl;
		return false;
	}

	ENGINE_PRINT("[FileUtilities] Successfully deleted file: " , p.generic_string(), "\n");
	return true;
}
