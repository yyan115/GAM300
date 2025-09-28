#pragma once
#include <string>
#include <filesystem>
#include "../Engine.h"

class FileUtilities {
public:
	ENGINE_API static bool RemoveFile(const std::string& filePath);
	const static std::filesystem::path& GetSolutionRootDir();
	static bool RemoveFromSolutionRootDir(const std::string& filePath);

private:
	static std::filesystem::path solutionRootDir;
};