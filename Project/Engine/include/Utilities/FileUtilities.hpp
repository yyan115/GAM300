#pragma once
#include <string>
#include <filesystem>
#include "../Engine.h"

class FileUtilities {
public:
	ENGINE_API static bool RemoveFile(const std::string& filePath);
};