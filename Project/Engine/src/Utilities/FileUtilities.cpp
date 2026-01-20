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

	return true;
}

//const std::filesystem::path& FileUtilities::GetSolutionRootDir() {
//	if (solutionRootDir.empty()) {
//		auto dir = std::filesystem::current_path();
//		bool found = false;
//		while (!dir.empty()) {
//			for (auto& entry : std::filesystem::directory_iterator(dir)) {
//				if (entry.path().extension() == ".sln") {
//					solutionRootDir = dir;
//					found = true;
//					break;
//				}
//			}
//
//			if (found)
//				break;
//
//			dir = dir.parent_path();
//		}
//	}
//
//	return solutionRootDir;
//}

//bool FileUtilities::RemoveFromSolutionRootDir(const std::string& filePath) {
//	std::filesystem::path p(GetSolutionRootDir() / filePath);
//	if (std::filesystem::exists(p) == false) {
//		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] WARNING: Attempted to delete non-existent file: ", p.generic_string(), "\n");
//		return true; // Consider it a success since the file doesn't exist.
//	}
//
//	std::error_code ec;
//	bool success = std::filesystem::remove(p, ec);
//	if (!success || ec) {
//		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[FileUtilities] ERROR: Failed to delete file: ", p.generic_string(), " (", ec.message(), ")", "\n");
//		return false;
//	}
//
//	ENGINE_PRINT("[FileUtilities] Successfully deleted file: " , p.generic_string(), "\n");
//	return true;
//}

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

bool FileUtilities::StrictExists(const std::filesystem::path& p) {
    // 1. Basic check: if the OS says it doesn't exist, we are done.
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return false;

    // Determine where we start scanning
    std::filesystem::path current_scan_dir;
    auto it = p.begin();

    // Handle Absolute vs Relative paths
    if (p.is_absolute()) {
        current_scan_dir = p.root_path();

        // Skip the root parts (e.g., "C:" and "\") in the iterator
        // because we don't need to check if "C:" is uppercase (it's virtual)
        if (it != p.end() && *it == p.root_name()) ++it;
        if (it != p.end() && *it == p.root_directory()) ++it;
    }
    else {
        // Relative path: Start at the current working directory
        current_scan_dir = std::filesystem::current_path();
    }

    // Iterate over every component of the input path
    for (; it != p.end(); ++it) {
        std::filesystem::path part = *it;

        // 2. Handle "." (Current Dir) - Just skip, it's valid
        if (part == ".") continue;

        // 3. Handle ".." (Parent Dir) - Move up, don't check string
        if (part == "..") {
            current_scan_dir = current_scan_dir.parent_path();
            continue;
        }

        // 4. Strict Check: Search for 'part' in 'current_scan_dir'
        bool found_exact = false;

        // Safety: Ensure we are actually looking inside a directory
        if (!std::filesystem::is_directory(current_scan_dir, ec)) return false;

        for (const auto& entry : std::filesystem::directory_iterator(current_scan_dir)) {
            // entry.path().filename() returns the ACTUAL on-disk casing
            if (entry.path().filename() == part) {
                found_exact = true;
                break;
            }
        }

        if (!found_exact) return false; // Case mismatch or file missing

        // Advance to the next folder level
        current_scan_dir /= part;
    }

    return true;
}

