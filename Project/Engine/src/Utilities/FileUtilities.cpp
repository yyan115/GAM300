#include "pch.h"

#include <iostream>
#include "Utilities/FileUtilities.hpp"

bool FileUtilities::RemoveFile(const std::string& filePath) {
	std::filesystem::path p(filePath);
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

	std::cout << "[FileUtilities] Successfully deleted file: " << p.generic_string() << std::endl;
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
		std::cerr << "[FileUtilities] Copy failed: " << e.what() << std::endl;
		return false;
	}

	return true;
}
