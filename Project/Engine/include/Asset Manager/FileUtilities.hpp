#pragma once
#include <string>
#include <filesystem>

class FileUtilities {
public:
	static bool RemoveFile(const std::string& filePath);
};