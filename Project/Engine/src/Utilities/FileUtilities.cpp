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
		return true; // Consider it a success since the file doesn't exist.
	}

	std::error_code ec;
	bool success = std::filesystem::remove(p, ec);
	if (!success || ec) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] ERROR: Failed to delete file: " , p.generic_string() , " (" , ec.message() , ")\n");
		return false;
	}
	ENGINE_PRINT("[FileUtilities] Successfully deleted file: ", p.generic_string(), "\n");

	//std::cout << "[FileUtilities] Successfully deleted file: " << p.generic_string() << std::endl;

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
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] WARNING: Attempted to delete non-existent file: ", p.generic_string(), "\n");
		return true; // Consider it a success since the file doesn't exist.
	}

	std::error_code ec;
	bool success = std::filesystem::remove(p, ec);
	if (!success || ec) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] ERROR: Failed to delete file: ", p.generic_string(), " (", ec.message(), ")", "\n");
		return false;
	}

	ENGINE_PRINT("[FileUtilities] Successfully deleted file: " , p.generic_string(), "\n");
	return true;
}

bool FileUtilities::CopyFile(const std::string& srcPath, const std::string& dstPath) {
	// Ensure parent directories exist.
	std::filesystem::path p(dstPath);
	std::filesystem::create_directories(p.parent_path());
	try {
		std::filesystem::copy_file(srcPath, dstPath,
			std::filesystem::copy_options::overwrite_existing);
	}
	catch (const std::filesystem::filesystem_error& e) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] Copy failed: ", e.what(), "\n");
		return false;
	}

	return true;
}

bool FileUtilities::CopyFileW(const std::string& srcPath, const std::string& dstPath) {
	return CopyFile(srcPath, dstPath);
}

std::filesystem::path FileUtilities::SanitizePathForAndroid(const std::filesystem::path& input) {
	std::filesystem::path result;

	for (const auto& part : input) {
		std::string name = part.string();

		// Only adjust directory names, not the root name or filename extension
		if (!name.empty() && name.front() == '_') {
			name.erase(0, 1); // remove leading underscore
		}

		for (char& c : name) {
			if (static_cast<unsigned char>(c) > 127) {
				c = '0'; // replace non-ASCII with 0
			}
		}
		result /= name;
	}

	return result;
}

