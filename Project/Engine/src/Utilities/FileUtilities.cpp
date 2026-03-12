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

// Sanitizes a single file or folder name (strips non-ASCII and forbidden chars)
std::string FileUtilities::SanitizeFileName(const std::string& name) {
    std::string safeName = name;

    // Lambda to identify invalid or non-standard characters
    auto isInvalid = [](unsigned char c) {
        // Keep standard alphanumeric characters (A-Z, a-z, 0-9)
        if (std::isalnum(c)) return false;
        // Keep safe, common symbols
        if (c == ' ' || c == '.' || c == '-' || c == '_') return false;
        // Flag everything else (including extended ASCII replacement chars and forbidden Windows chars < > : " / \ | ? *)
        return true;
        };

    // Replace all invalid characters with an underscore
    std::replace_if(safeName.begin(), safeName.end(), isInvalid, '_');

    // Windows restriction: Filenames cannot end with a space or a period
    while (!safeName.empty() && (safeName.back() == ' ' || safeName.back() == '.')) {
        safeName.pop_back();
    }

    return safeName.empty() ? "unnamed_file" : safeName;
}

// Helper to sanitize only the filename portion of a full absolute/relative path
std::string FileUtilities::SanitizeFilePath(const std::string& fullPath) {
    std::filesystem::path pathObj(fullPath);

    // Extract the directory and the original filename
    std::filesystem::path dir = pathObj.parent_path();
    std::string originalFilename = pathObj.filename().string();

    // Sanitize only the filename
    std::string safeFilename = SanitizeFileName(originalFilename);

    // Recombine the path with the sanitized filename
    std::filesystem::path finalPathObj = dir / safeFilename;

    // 1. Use generic_string() to prefer forward slashes (standard C++ feature)
    std::string finalPathStr = finalPathObj.generic_string();

    // 2. Explicitly replace any remaining backslashes with forward slashes 
    // (This guarantees consistency even if the input 'fullPath' had mixed separators)
    std::replace(finalPathStr.begin(), finalPathStr.end(), '\\', '/');

    return finalPathStr;
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

bool FileUtilities::StrictDirectoryExists(std::filesystem::path p) {
    std::error_code ec;

    // If the path is a file, chop off the filename so we are only checking the directory
    if (std::filesystem::is_regular_file(p, ec)) {
        p = p.parent_path();
    }

    // Fast fail: if it's not a directory at this point, it's invalid
    if (!std::filesystem::is_directory(p, ec)) return false;

    std::filesystem::path current_scan_dir;
    auto it = p.begin();

    if (p.is_absolute()) {
        current_scan_dir = p.root_path();
        if (it != p.end() && *it == p.root_name()) ++it;
        if (it != p.end() && *it == p.root_directory()) ++it;
    }
    else {
        current_scan_dir = std::filesystem::current_path();
    }

    for (; it != p.end(); ++it) {
        std::filesystem::path part = *it;

        if (part.empty() || part == ".") continue;

        if (part == "..") {
            current_scan_dir = current_scan_dir.parent_path();
            continue;
        }

        bool found_exact = false;

        if (!std::filesystem::is_directory(current_scan_dir, ec)) return false;

        for (const auto& entry : std::filesystem::directory_iterator(current_scan_dir)) {
            if (entry.path().filename() == part) {
                found_exact = true;
                break;
            }
        }

        if (!found_exact) return false;

        current_scan_dir /= part;
    }

    return true;
}