#pragma once
// Prevent Windows.h from defining CopyFile macro
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif

#include <string>
#include <filesystem>
#include "../Engine.h"

// Undefine potential Windows macros
#ifdef CopyFile
#undef CopyFile
#endif

class FileUtilities {
public:
	ENGINE_API static bool RemoveFile(const std::string& filePath);
	ENGINE_API static const std::filesystem::path& GetSolutionRootDir();
	ENGINE_API static bool RemoveFromSolutionRootDir(const std::string& filePath);
	ENGINE_API static bool CopyFile(const std::string& srcPath, const std::string& dstPath);
	// Windows compatibility alias - Windows.h macros map CopyFile to CopyFileW
	ENGINE_API static bool CopyFileW(const std::string& srcPath, const std::string& dstPath);

private:
	static std::filesystem::path solutionRootDir;
};